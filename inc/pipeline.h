#pragma once
#include <pthread.h>
#include <stdbool.h>

#include "config.h"
#include "queue.h"
#include "scheduler.h"

#include "wireless.h"
#include "wired_client.h"
#include "state_manager.h"
#include "led.h"

typedef struct {
  app_config_t cfg;

  // Queues
  bq_t Q_wl1_raw;     // wl1_msg_t*
  bq_t Q_sm_events;   // sm_event_t*
  bq_t Q_tx_cmd;      // tx_cmd_t*
  bq_t Q_rsu3_in;     // rsu3p_msg_t*
  bq_t Q_air;         // uint8_t[256]*

  // Scheduler thread
  scheduler_t sched;
  pthread_t th_sched;

  // HW
  led_handle_t *led;

  // IO
  wireless_t wireless;      // UDP RX/TX 묶음
  wired_client_t wc;        // TCP + TxManager

  // State machine
  state_manager_t sm;

  // Workers
  pthread_t th_wl1_worker;
  pthread_t th_rsu3_dispatch;

  bool running;
} pipeline_t;

int  pipeline_start(pipeline_t *p);
void pipeline_stop(pipeline_t *p);