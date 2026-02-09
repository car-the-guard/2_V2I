#include "wired_client.h"
#include "security.h"
#include "log.h"       // 기존 로그 (LOGW 등)
#include "debug.h"     // [추가] 정밀 시간 측정을 위한 커스텀 디버그 모듈
#include "timeutil.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// 간단 pending 엔트리 (재전송 관리용)
typedef struct {
  bool used;
  uint32_t msg_id;
  uint64_t sent_ms;
  int retry;
  tx_cmd_wired_t *cmd;
} pending_t;

static int tcp_connect_bind(const app_config_t *cfg) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return -1;

  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  // local bind: 20905
  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(cfg->local_port);
  local.sin_addr.s_addr = inet_addr("0.0.0.0");
  if (bind(s, (struct sockaddr*)&local, sizeof(local)) < 0) {
    LOGW("tcp bind local failed: %d", errno);
    close(s);
    return -1;
  }

  struct sockaddr_in srv;
  memset(&srv, 0, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_port = htons(cfg->server_port);
  srv.sin_addr.s_addr = inet_addr(cfg->server_ip);

  if (connect(s, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
    LOGW("tcp connect failed: %d", errno);
    close(s);
    return -1;
  }
  
  // [DEBUG] 연결 성공 시점 기록
  DBG_INFO("TCP Connected to Server %s:%d", cfg->server_ip, cfg->server_port);
  return s;
}

// RX thread: RSU-3(Packet) 수신 -> Strip -> RSU-3'(Payload)
static void* tcp_rx_thread(void *arg) {
    wired_client_t *wc = (wired_client_t*)arg;
    wc->running = true;

    uint8_t buf[256];

    while (wc->running) {
        // RSU-3 Packet Size (64 Bytes) 수신
        ssize_t n = recv(wc->sock, buf, sizeof(rsu3_packet_t), MSG_WAITALL);
        if (n <= 0) {
            if (errno == EINTR) continue;
            LOGW("tcp recv end/err: %d", errno);
            break;
        }

        if (n != sizeof(rsu3_packet_t)) continue;

        // [DEBUG] 서버로부터 데이터 수신 완료 시점 (RX Start)
        DBG_INFO("[RX] RSU-3 Packet received (%ld bytes)", n);

        const rsu3_packet_t *pkt = (rsu3_packet_t*)buf;
        
        // [RX Strip] RSU-3 -> RSU-3'
        rsu3_payload_t *payload = (rsu3_payload_t*)calloc(1, sizeof(rsu3_payload_t));
        if (sec_wired_rx_strip(pkt, payload)) {
            // [DEBUG] 큐에 넣기 직전
            DBG_INFO("[RX] Pushing RSU-3 Payload to Queue");
            bq_push(wc->rsu3_out_q, payload);
        } else {
            free(payload);
        }
    }
    return NULL;
}

// TxManager: RSU-2'(Payload) -> Wrap -> RSU-2(Packet) -> Send
static void* tcp_tx_manager_thread(void *arg) {
    wired_client_t *wc = (wired_client_t*)arg;

    // pending 로직은 간단히 유지
    pending_t pend[64];
    memset(pend, 0, sizeof(pend));

    while (wc->running) {
        // StateManager가 보낸 RSU-2'(Payload) 가져오기
        tx_cmd_wired_t *cmd = (tx_cmd_wired_t*)bq_pop(wc->tx_cmd_q);
        if (!cmd) {
            if (!wc->running) break;
            continue;
        }

        // [DEBUG] 큐에서 데이터를 꺼낸 시점 (StateManager -> WiredClient 이동 시간 확인용)
        DBG_INFO("[TX] Pop cmd from Queue. Addr: %p", cmd);

        if (cmd->rsu2p) {
            // [TX Wrap] RSU-2' -> RSU-2
            rsu2_packet_t pkt;
            memset(&pkt, 0, sizeof(pkt));
            
            if (sec_wired_tx_wrap(cmd->rsu2p, &pkt)) {
                // [DEBUG] 소켓 전송 시작 (시스템 콜 호출 직전)
                DBG_INFO("[TX] Calling send() ... Size: %lu", sizeof(pkt));
                
                send(wc->sock, &pkt, sizeof(pkt), 0);
                
                // [DEBUG] 소켓 전송 완료 (커널 버퍼 복사 완료)
                DBG_INFO("[TX] send() Return (Sent to Kernel Buffer)");
            }
            free(cmd->rsu2p);
        }
        free(cmd);
    }
    return NULL;
}

int wired_client_start(wired_client_t *wc, const app_config_t *cfg, bq_t *tx_cmd_q, bq_t *rsu3_out_q) {
  memset(wc, 0, sizeof(*wc));
  wc->cfg = cfg;
  wc->tx_cmd_q = tx_cmd_q;
  wc->rsu3_out_q = rsu3_out_q;

  wc->ack_q = (bq_t*)calloc(1, sizeof(bq_t));
  if (!wc->ack_q) return -1;
  if (bq_init(wc->ack_q, 1024, Q_DROP_TAIL) != 0) return -1;

  wc->sock = tcp_connect_bind(cfg);
  if (wc->sock < 0) return -1;

  wc->running = true;
  if (pthread_create(&wc->th_rx, NULL, tcp_rx_thread, wc) != 0) return -1;
  if (pthread_create(&wc->th_tx, NULL, tcp_tx_manager_thread, wc) != 0) return -1;

  return 0;
}

void wired_client_stop(wired_client_t *wc) {
  if (!wc) return;
  wc->running = false;
  if (wc->sock > 0) close(wc->sock);

  bq_stop(wc->tx_cmd_q);
  bq_stop(wc->ack_q);

  pthread_join(wc->th_rx, NULL);
  pthread_join(wc->th_tx, NULL);

  bq_destroy(wc->ack_q);
  free(wc->ack_q);
}