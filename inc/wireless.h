#pragma once
#include <pthread.h>
#include <stdbool.h>
#include "config.h"
#include "queue.h"

/*
 * wireless.c는 wireless_rx + wireless_tx를 묶은 모듈.
 * - RX: UDP recvfrom(256B) -> wl1_msg_t* 생성 -> out_rx_q로 push
 * - TX: in_tx_q에서 uint8_t[256]* pop -> UDP sendto (브로드캐스트)
 */

typedef struct {
  pthread_t th_rx;
  pthread_t th_tx;
  bool running;

  int sock_rx;
  int sock_tx;

  const app_config_t *cfg;

  bq_t *out_rx_q;  // wl1_msg_t*
  bq_t *in_tx_q;   // uint8_t[256]*
} wireless_t;

int  wireless_start(wireless_t *w, const app_config_t *cfg, bq_t *out_rx_q, bq_t *in_tx_q);
void wireless_stop(wireless_t *w);
