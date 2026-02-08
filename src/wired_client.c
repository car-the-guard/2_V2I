// io/wired_client.c
#include "wired_client.h"
#include "log.h"
#include "timeutil.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// 간단 pending 엔트리 (스텁)
typedef struct {
  bool used;
  uint32_t msg_id;
  uint64_t sent_ms;
  int retry;
  tx_cmd_t *cmd; // 재전송용 보관
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
    // bind 실패해도 진행 가능한 환경도 있음(포트 점유 등) -> 여기서는 실패로 둠
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
  return s;
}

// RX thread: (스텁) ACK / RSU-3 수신 파싱 자리
static void* tcp_rx_thread(void *arg) {
  wired_client_t *wc = (wired_client_t*)arg;
  wc->running = true;

  uint8_t buf[512];
  while (wc->running) {
    ssize_t n = recv(wc->sock, buf, sizeof(buf), 0);
    if (n <= 0) {
      if (errno == EINTR) continue;
      LOGW("tcp recv end/err: %d", errno);
      break;
    }
    // TODO: 프로토콜 파싱
    // 스텁: "ACKxxxx" 가 오면 ack 처리
    if (n >= 7 && memcmp(buf, "ACK", 3) == 0) {
      ack_msg_t *ack = (ack_msg_t*)calloc(1, sizeof(*ack));
      ack->msg_id = 1; // TODO: 실제 msg_id 파싱
      bq_push(wc->ack_q, ack);
      continue;
    }

    // 스텁: RSU-3 이벤트 하나 생성
    rsu3p_msg_t *r = (rsu3p_msg_t*)calloc(1, sizeof(*r));
    r->accident_id = 1001;
    r->cmd_onoff = 1;
    r->rx_ms = now_ms_monotonic();
    bq_push(wc->rsu3_out_q, r);
  }
  return NULL;
}

// TxManager: send + pending + timeout 재전송 (단일 스레드)
static void* tcp_tx_manager_thread(void *arg) {
  wired_client_t *wc = (wired_client_t*)arg;

  // ack queue는 내부 생성
  pending_t pend[64];
  memset(pend, 0, sizeof(pend));

  const uint64_t ACK_TIMEOUT_MS = 300;  // TODO 튜닝
  const int MAX_RETRY = 5;

  while (wc->running) {
    // 1) ack 처리(논블로킹 느낌을 위해 pop 전에 running 체크)
    ack_msg_t *ack = (ack_msg_t*)bq_pop(wc->ack_q);
    if (ack) {
      for (int i=0;i<64;i++) {
        if (pend[i].used && pend[i].msg_id == ack->msg_id) {
          pend[i].used = false;
          // cmd 메모리 해제
          if (pend[i].cmd) {
            if (pend[i].cmd->rsu2p) free(pend[i].cmd->rsu2p);
            free(pend[i].cmd);
          }
          break;
        }
      }
      free(ack);
    }

    // 2) 새 전송 명령 가져오기(있으면 send)
    tx_cmd_t *cmd = (tx_cmd_t*)bq_pop(wc->tx_cmd_q);
    if (cmd) {
      // TODO: RSU-2 직렬화 + 유선보안 wrap
      uint8_t payload[64];
      size_t len = 0;
      memcpy(payload, &cmd->rsu2p->accident_id, sizeof(cmd->rsu2p->accident_id)); len += 4;

      send(wc->sock, payload, (int)len, 0);

      // pending 등록
      int slot = -1;
      for (int i=0;i<64;i++) if (!pend[i].used) { slot=i; break; }
      if (slot >= 0) {
        pend[slot].used = true;
        pend[slot].msg_id = cmd->msg_id;
        pend[slot].sent_ms = now_ms_monotonic();
        pend[slot].retry = 0;
        pend[slot].cmd = cmd; // ack 받을 때까지 보관
      } else {
        // pending 꽉 차면 드랍(혹은 block)
        if (cmd->rsu2p) free(cmd->rsu2p);
        free(cmd);
      }
    }

    // 3) timeout 검사 및 재전송
    uint64_t now = now_ms_monotonic();
    for (int i=0;i<64;i++) {
      if (!pend[i].used) continue;
      if (now - pend[i].sent_ms < ACK_TIMEOUT_MS) continue;

      if (pend[i].retry >= MAX_RETRY) {
        LOGW("ACK timeout drop msg_id=%u", pend[i].msg_id);
        pend[i].used = false;
        if (pend[i].cmd) {
          if (pend[i].cmd->rsu2p) free(pend[i].cmd->rsu2p);
          free(pend[i].cmd);
        }
        continue;
      }

      // 재전송(스텁)
      uint8_t payload[64];
      size_t len = 0;
      memcpy(payload, &pend[i].cmd->rsu2p->accident_id, 4); len += 4;
      send(wc->sock, payload, (int)len, 0);

      pend[i].retry++;
      pend[i].sent_ms = now;
    }
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
