#pragma once
#include <pthread.h>
#include <stdbool.h>
#include "config.h"
#include "queue.h"
#include "types.h"

typedef struct {
  bool running;
  const app_config_t *cfg;

  // Outgoing (RSU -> Server)
  int sock_out;
  pthread_t th_tx;
  pthread_t th_rx_ack; // 즉시 응답 수신용

  // Incoming (Server -> RSU)
  int sock_in_listen;
  pthread_t th_cmd_srv; // 명령 수신 서버용

  bq_t *tx_cmd_q;   // tx_cmd_t*
  bq_t *rsu3_out_q; // rsu3p_msg_t* (rx -> pipeline/state)
} wired_client_t;

int wired_client_start(wired_client_t *wc, const app_config_t *cfg, bq_t *tx_cmd_q, bq_t *rsu3_out_q);
void wired_client_stop(wired_client_t *wc);