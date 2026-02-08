// io/wired_client.h
#pragma once
#include <pthread.h>
#include <stdbool.h>
#include "config.h"
#include "queue.h"
#include "types.h"

typedef struct {
  pthread_t th_tx;
  pthread_t th_rx;
  bool running;
  int sock;
  const app_config_t *cfg;

  bq_t *tx_cmd_q;   // tx_cmd_t*
  bq_t *ack_q;      // ack_msg_t*  (rx -> tx)
  bq_t *rsu3_out_q; // rsu3p_msg_t* (rx -> pipeline/state)
} wired_client_t;

int wired_client_start(wired_client_t *wc, const app_config_t *cfg, bq_t *tx_cmd_q, bq_t *rsu3_out_q);
void wired_client_stop(wired_client_t *wc);
