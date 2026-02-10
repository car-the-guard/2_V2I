#include "wired_client.h"
#include "security.h"
#include "log.h"
#include "debug.h"
#include "timeutil.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
// 1. [Outgoing] RSU -> Server (사고 보고용 클라이언트)
// -----------------------------------------------------------------------------
static int tcp_connect_to_server(const app_config_t *cfg) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return -1;

  // [수정] 20905번 Bind 제거! 
  // RSU가 서버로 보낼 때는 OS가 아무 빈 포트나 쓰게 둡니다.
  // 20905번은 아래 "수신 서버"가 써야 하기 때문입니다.

  struct sockaddr_in srv;
  memset(&srv, 0, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_port = htons(cfg->server_port); // 20615
  srv.sin_addr.s_addr = inet_addr(cfg->server_ip);

  if (connect(s, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
    LOGW("tcp connect failed: %d", errno);
    close(s);
    return -1;
  }
  
  DBG_INFO("TCP Connected to Server %s:%d (Outgoing)", cfg->server_ip, cfg->server_port);
  return s;
}

// [Thread] 서버가 보내는 "즉시 응답(ACK)" 수신 (기존 연결 유지)
static void* tcp_rx_ack_thread(void *arg) {
    wired_client_t *wc = (wired_client_t*)arg;
    uint8_t buf[256];

    while (wc->running) {
        // RSU-3 Packet Size (64 Bytes) 수신
        ssize_t n = recv(wc->sock_out, buf, sizeof(rsu3_packet_t), MSG_WAITALL);
        if (n <= 0) {
            // 연결 끊기면 재연결 로직이 필요하지만, 여기선 로그만 찍고 종료
            if (wc->running) LOGW("Outgoing connection recv error: %d", errno);
            break;
        }

        if (n != sizeof(rsu3_packet_t)) continue;

        // [RX Strip] RSU-3 -> RSU-3'
        rsu3_packet_t *pkt = (rsu3_packet_t*)buf;
        rsu3_payload_t *payload = (rsu3_payload_t*)calloc(1, sizeof(rsu3_payload_t));
        
        if (sec_wired_rx_strip(pkt, payload)) {
            // 즉시 응답(ON 확인)도 상태 관리에 반영
            bq_push(wc->rsu3_out_q, payload);
        } else {
            free(payload);
        }
    }
    return NULL;
}

// [Thread] 사고 패킷 전송 (TX Manager)
static void* tcp_tx_manager_thread(void *arg) {
    wired_client_t *wc = (wired_client_t*)arg;

    while (wc->running) {
        tx_cmd_wired_t *cmd = (tx_cmd_wired_t*)bq_pop(wc->tx_cmd_q);
        if (!cmd) {
            if (!wc->running) break;
            continue;
        }

        if (cmd->rsu2p) {
            rsu2_packet_t pkt;
            memset(&pkt, 0, sizeof(pkt));
            
            if (sec_wired_tx_wrap(cmd->rsu2p, &pkt)) {
                send(wc->sock_out, &pkt, sizeof(pkt), 0);
                DBG_INFO("[TX] Sent Accident Report to Server");
            }
            free(cmd->rsu2p);
        }
        free(cmd);
    }
    return NULL;
}


// -----------------------------------------------------------------------------
// 2. [Incoming] Server -> RSU (명령 수신용 서버) - 핵심 추가!!
// -----------------------------------------------------------------------------
static void* tcp_command_server_thread(void *arg) {
    wired_client_t *wc = (wired_client_t*)arg;
    
    // 1. 수신용 소켓 생성
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) return NULL;

    int yes = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(wc->cfg->local_port); // 20905
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE("Command Server Bind Failed (Port %d): %d", wc->cfg->local_port, errno);
        close(listen_sock);
        return NULL;
    }

    listen(listen_sock, 5);
    LOGI("Command Server Listening on Port %d (Waiting for OFF signal)", wc->cfg->local_port);
    wc->sock_in_listen = listen_sock;

    while (wc->running) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        
        // 2. 서버 접속 대기 (Accept)
        int conn = accept(listen_sock, (struct sockaddr*)&cli_addr, &len);
        if (conn < 0) {
            if (errno == EINTR) continue;
            break; // 종료
        }

        // DBG_INFO("Server connected to send command!");

        // 3. 명령 패킷 수신 (64 Bytes)
        uint8_t buf[256];
        ssize_t n = recv(conn, buf, sizeof(rsu3_packet_t), MSG_WAITALL);
        
        if (n == sizeof(rsu3_packet_t)) {
            rsu3_packet_t *pkt = (rsu3_packet_t*)buf;
            rsu3_payload_t *payload = (rsu3_payload_t*)calloc(1, sizeof(rsu3_payload_t));
            
            if (sec_wired_rx_strip(pkt, payload)) {
                LOGI("[CMD] Received Command from Server (Flag: 0x%04X)", payload->server_info.acc_flag);
                // State Manager에게 전달 -> 여기서 LED 꺼짐!
                bq_push(wc->rsu3_out_q, payload);
            } else {
                free(payload);
            }
        }
        
        // 명령 하나 받고 바로 끊는 구조 (HTTP 처럼)
        close(conn);
    }
    
    close(listen_sock);
    return NULL;
}


// -----------------------------------------------------------------------------
// 초기화 및 종료
// -----------------------------------------------------------------------------
int wired_client_start(wired_client_t *wc, const app_config_t *cfg, bq_t *tx_cmd_q, bq_t *rsu3_out_q) {
  memset(wc, 0, sizeof(*wc));
  wc->cfg = cfg;
  wc->tx_cmd_q = tx_cmd_q;
  wc->rsu3_out_q = rsu3_out_q;

  wc->running = true;

  // 1. 서버로 접속 (Outgoing)
  wc->sock_out = tcp_connect_to_server(cfg);
  if (wc->sock_out < 0) {
      LOGW("Failed to connect to server (Offline Mode)");
      // 실패해도 수신 서버는 켜야 함
  } else {
      if (pthread_create(&wc->th_tx, NULL, tcp_tx_manager_thread, wc) != 0) return -1;
      if (pthread_create(&wc->th_rx_ack, NULL, tcp_rx_ack_thread, wc) != 0) return -1;
  }

  // 2. 서버로부터 접속 대기 (Incoming Server) - New!
  if (pthread_create(&wc->th_cmd_srv, NULL, tcp_command_server_thread, wc) != 0) return -1;

  return 0;
}

void wired_client_stop(wired_client_t *wc) {
  if (!wc) return;
  wc->running = false;
  
  if (wc->sock_out > 0) close(wc->sock_out);
  if (wc->sock_in_listen > 0) {
      shutdown(wc->sock_in_listen, SHUT_RDWR); // accept 깨우기
      close(wc->sock_in_listen);
  }

  // 쓰레드 종료 대기 (단순화)
  // pthread_join... 
}