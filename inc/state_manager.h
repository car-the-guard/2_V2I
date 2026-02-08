#pragma once
#include <pthread.h>
#include <stdbool.h>

#include "config.h"
#include "queue.h"
#include "scheduler.h"
#include "led.h"

typedef struct {
  pthread_t th;
  bool running;

  const app_config_t *cfg;
  led_handle_t *led;

  bq_t *in_ev_q;       // sm_event_t*
  bq_t *to_tx_cmd_q;   // tx_cmd_t*
  bq_t *to_air_q;      // uint8_t[256]*

  scheduler_t *sched;
} state_manager_t;

int  state_manager_start(state_manager_t *sm,
                         const app_config_t *cfg,
                         bq_t *in_ev_q,
                         bq_t *to_tx_cmd_q,
                         bq_t *to_air_q,
                         scheduler_t *sched,
                         led_handle_t *led);

void state_manager_stop(state_manager_t *sm);
