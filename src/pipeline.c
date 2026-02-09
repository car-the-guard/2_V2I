#include "pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "types.h"

#include "filter.h"
#include "security.h"
#include "packet.h"
#include "debug.h"

// WL-1 Worker: [Raw Q] -> [Filter] -> [Strip] -> [Packet Conv] -> [SM Event Q]
static void* wl1_worker_thread(void *arg) {
    pipeline_t *p = (pipeline_t*)arg;

    while (p->running) {
        wl1_packet_t *pkt = (wl1_packet_t*)bq_pop(&p->Q_wl1_raw);
        if (!pkt) break;

        uint32_t dist = 0;
        DBG_INFO("[STEP 2] Worker Pop. Addr: %p", pkt);
        // 1. Filter (Raw Packet 검사)
        if (!filter_pass_all(pkt, p->cfg.rsu_id, &dist)) {
            free(pkt);
            continue;
        }

        // 2. Wireless RX Strip (Packet -> Payload)
        wl1_payload_t stripped;
        if (!sec_wireless_rx_strip(pkt, &stripped)) {
            free(pkt);
            continue;
        }

        // 3. Packet Convert (WL-1' -> RSU-2')
        rsu2_payload_t *rsu2p = calloc(1, sizeof(rsu2_payload_t));
        if (!packet_wl1_to_rsu2(&stripped, p->cfg.rsu_id, dist, rsu2p)) {
            free(rsu2p);
            free(pkt);
            continue;
        }

        // 4. Send to StateManager
        sm_event_t *ev = calloc(1, sizeof(sm_event_t));
        ev->type = EV_WL1_RX;
        ev->u.rsu2p = rsu2p;
        DBG_INFO("[STEP 3] Push to SM Queue");
        bq_push(&p->Q_sm_events, ev);

        free(pkt);
    }
    return NULL;
}

// RSU-3 Dispatch: [RSU-3 Q] -> [SM Event Q]
static void* rsu3_dispatch_thread(void *arg) {
    pipeline_t *p = (pipeline_t*)arg;
    while (p->running) {
        rsu3_payload_t *r = (rsu3_payload_t*)bq_pop(&p->Q_rsu3_in);
        if (!r) break;

        sm_event_t *ev = calloc(1, sizeof(sm_event_t));
        ev->type = EV_RSU3_RX;
        ev->u.rsu3p = r;
        bq_push(&p->Q_sm_events, ev);
    }
    return NULL;
}

int pipeline_start(pipeline_t *p) {
  memset(p, 0, sizeof(*p));
  load_default_config(&p->cfg);

  // Queues
  if (bq_init(&p->Q_wl1_raw,   1024, Q_DROP_TAIL) != 0) return -1;
  if (bq_init(&p->Q_sm_events, 2048, Q_BLOCK)     != 0) return -1;
  if (bq_init(&p->Q_tx_cmd,    1024, Q_BLOCK)     != 0) return -1;
  if (bq_init(&p->Q_rsu3_in,   1024, Q_BLOCK)     != 0) return -1;
  if (bq_init(&p->Q_air,       1024, Q_DROP_HEAD) != 0) return -1;

  // Scheduler
  if (scheduler_init(&p->sched, 2048) != 0) return -1;
  if (pthread_create(&p->th_sched, NULL, scheduler_thread, &p->sched) != 0) return -1;

  // LED (실패해도 계속 진행)
  p->led = led_open(p->cfg.gpiochip, p->cfg.led_line);
  if (!p->led) LOGW("LED open failed (continue)");

  p->running = true;

  // Wireless (UDP RX/TX)
  if (wireless_start(&p->wireless, &p->cfg, &p->Q_wl1_raw, &p->Q_air) != 0) {
    LOGE("wireless_start failed");
    return -1;
  }

  // Wired (TCP to server) - 실패 시에도 “오프라인 모드”로 진행 가능
  if (wired_client_start(&p->wc, &p->cfg, &p->Q_tx_cmd, &p->Q_rsu3_in) != 0) {
    LOGW("wired_client_start failed (offline mode)");
  }

  // State manager
  if (state_manager_start(&p->sm, &p->cfg,
                          &p->Q_sm_events, &p->Q_tx_cmd, &p->Q_air,
                          &p->sched, p->led) != 0) {
    LOGE("state_manager_start failed");
    return -1;
  }

  // Workers
  if (pthread_create(&p->th_wl1_worker, NULL, wl1_worker_thread, p) != 0) return -1;
  if (pthread_create(&p->th_rsu3_dispatch, NULL, rsu3_dispatch_thread, p) != 0) return -1;

  LOGI("pipeline started");
  return 0;
}

void pipeline_stop(pipeline_t *p) {
  if (!p) return;
  p->running = false;

  // stop queues first to wake blockers
  bq_stop(&p->Q_wl1_raw);
  bq_stop(&p->Q_sm_events);
  bq_stop(&p->Q_tx_cmd);
  bq_stop(&p->Q_rsu3_in);
  bq_stop(&p->Q_air);

  // stop modules
  wireless_stop(&p->wireless);
  wired_client_stop(&p->wc);

  state_manager_stop(&p->sm);

  // stop scheduler
  scheduler_stop(&p->sched);
  pthread_join(p->th_sched, NULL);
  scheduler_destroy(&p->sched);

  // join workers
  pthread_join(p->th_wl1_worker, NULL);
  pthread_join(p->th_rsu3_dispatch, NULL);

  led_close(p->led);

  // destroy queues
  bq_destroy(&p->Q_wl1_raw);
  bq_destroy(&p->Q_sm_events);
  bq_destroy(&p->Q_tx_cmd);
  bq_destroy(&p->Q_rsu3_in);
  bq_destroy(&p->Q_air);

  LOGI("pipeline stopped");
}
