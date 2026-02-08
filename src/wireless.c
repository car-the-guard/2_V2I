#include "wireless.h"

#include "types.h"
#include "log.h"
#include "timeutil.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef WL1_TX_BCAST_IP
#define WL1_TX_BCAST_IP "255.255.255.255"
#endif

#ifndef WL1_TX_PORT
// TODO: 실제 WL-1 송신 포트로 바꿔야 함
#define WL1_TX_PORT 30001
#endif

static void* wireless_rx_thread(void *arg) {
  wireless_t *w = (wireless_t*)arg;

  while (w->running) {
    uint8_t buf[256];
    struct sockaddr_in src;
    socklen_t slen = sizeof(src);

    ssize_t n = recvfrom(w->sock_rx, buf, sizeof(buf), 0, (struct sockaddr*)&src, &slen);
    if (n < 0) {
      if (!w->running) break;
      if (errno == EINTR) continue;
      LOGW("wireless rx recvfrom failed: errno=%d", errno);
      continue;
    }
    if (n != 256) {
      // length 체크
      continue;
    }

    wl1_msg_t *m = (wl1_msg_t*)calloc(1, sizeof(*m));
    if (!m) continue;

    memcpy(m->raw, buf, 256);
    m->rx_ms = now_ms_monotonic();

    // TODO: 실제 WL-1 파싱 로직 (ttl/ver/msg_type/accident_id/...)
    // 스텁 값
    m->ttl = 3;
    m->ver = 1;
    m->msg_type = 0;
    m->accident_id = 1001;
    m->sender_id = 0xABC00001;
    m->severity = 3;
    m->direction = 1;
    m->lat = 37.0;
    m->lon = 127.0;

    if (!bq_push(w->out_rx_q, m)) {
      free(m);
    }
  }

  return NULL;
}

static void* wireless_tx_thread(void *arg) {
  wireless_t *w = (wireless_t*)arg;

  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port = htons(WL1_TX_PORT);
  dst.sin_addr.s_addr = inet_addr(WL1_TX_BCAST_IP);

  int yes = 1;
  setsockopt(w->sock_tx, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));

  while (w->running) {
    uint8_t *pkt = (uint8_t*)bq_pop(w->in_tx_q);
    if (!pkt) {
      // queue가 stop되면 NULL 가능
      if (!w->running) break;
      continue;
    }

    (void)sendto(w->sock_tx, pkt, 256, 0, (struct sockaddr*)&dst, sizeof(dst));
    free(pkt);
  }

  return NULL;
}

int wireless_start(wireless_t *w, const app_config_t *cfg, bq_t *out_rx_q, bq_t *in_tx_q) {
  if (!w || !cfg || !out_rx_q || !in_tx_q) return -1;

  memset(w, 0, sizeof(*w));
  w->cfg = cfg;
  w->out_rx_q = out_rx_q;
  w->in_tx_q = in_tx_q;
  w->running = true;

  // RX socket
  w->sock_rx = socket(AF_INET, SOCK_DGRAM, 0);
  if (w->sock_rx < 0) return -1;

  int yes = 1;
  setsockopt(w->sock_rx, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg->wl1_listen_port);
  addr.sin_addr.s_addr = inet_addr(cfg->wl1_bind_ip);

  if (bind(w->sock_rx, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    LOGE("wireless rx bind failed: errno=%d", errno);
    close(w->sock_rx);
    w->sock_rx = -1;
    return -1;
  }

  // TX socket
  w->sock_tx = socket(AF_INET, SOCK_DGRAM, 0);
  if (w->sock_tx < 0) {
    close(w->sock_rx);
    w->sock_rx = -1;
    return -1;
  }

  // threads
  if (pthread_create(&w->th_rx, NULL, wireless_rx_thread, w) != 0) {
    close(w->sock_rx);
    close(w->sock_tx);
    w->sock_rx = w->sock_tx = -1;
    return -1;
  }
  if (pthread_create(&w->th_tx, NULL, wireless_tx_thread, w) != 0) {
    w->running = false;
    close(w->sock_rx);
    close(w->sock_tx);
    pthread_join(w->th_rx, NULL);
    w->sock_rx = w->sock_tx = -1;
    return -1;
  }

  return 0;
}

void wireless_stop(wireless_t *w) {
  if (!w) return;

  w->running = false;

  // 소켓을 닫아서 recvfrom/sendto 블록을 깨움
  if (w->sock_rx >= 0) { close(w->sock_rx); w->sock_rx = -1; }
  if (w->sock_tx >= 0) { close(w->sock_tx); w->sock_tx = -1; }

  pthread_join(w->th_rx, NULL);
  pthread_join(w->th_tx, NULL);
}
