#include "state_manager.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "types.h"
#include "timeutil.h"
#include "packet.h"

// ---- 사고 테이블(간단 버전) ----
typedef struct {
  uint32_t accident_id;
  bool active;
  uint64_t expire_ms;
  rsu3p_msg_t last_rsu3;
} acc_ent_t;

// ---- 2초 tick 이벤트를 SM 큐로 넣는 타이머 콜백 ----
static void post_tick_event(void *arg) {
  bq_t *q = (bq_t*)arg;
  sm_event_t *ev = (sm_event_t*)calloc(1, sizeof(*ev));
  if (!ev) return;
  ev->type = EV_TIMER_2S_TICK;
  (void)bq_push(q, ev);
}

static void schedule_next_2s(scheduler_t *sched, bq_t *evq) {
  // 2초마다 tick
  (void)scheduler_add(sched, now_ms_monotonic() + 2000, post_tick_event, evq);
}

static void* sm_thread(void *arg) {
  state_manager_t *sm = (state_manager_t*)arg;
  sm->running = true;

  acc_ent_t table[256];
  memset(table, 0, sizeof(table));
  int n = 0;

  // 최초 tick 예약
  schedule_next_2s(sm->sched, sm->in_ev_q);

  while (sm->running) {
    sm_event_t *ev = (sm_event_t*)bq_pop(sm->in_ev_q);
    if (!ev) break;

    uint64_t now = now_ms_monotonic();

    if (ev->type == EV_FROM_WL1_RSU2P) {
      // WL-1 -> RSU-2' 들어온 이벤트
      // TODO: dedup 키 설계/중복관리 추가 가능
      tx_cmd_t *cmd = (tx_cmd_t*)calloc(1, sizeof(*cmd));
      if (cmd) {
        cmd->rsu2p = ev->u.rsu2p;  // ownership 이동
        cmd->msg_id = 1;          // TODO: 증가 카운터로 교체
        (void)bq_push(sm->to_tx_cmd_q, cmd);
      } else {
        free(ev->u.rsu2p);
      }
    }
    else if (ev->type == EV_FROM_SERVER_RSU3P) {
      // 서버 -> RSU-3' 들어온 이벤트
      rsu3p_msg_t *r = ev->u.rsu3p;

      int idx = -1;
      for (int i = 0; i < n; i++) {
        if (table[i].accident_id == r->accident_id) { idx = i; break; }
      }
      if (idx < 0 && n < 256) {
        idx = n++;
        table[idx].accident_id = r->accident_id;
      }

      if (idx >= 0) {
        table[idx].active = (r->cmd_onoff == 1);
        table[idx].expire_ms = now + 15000; // TODO: 정책에 맞게 갱신
        table[idx].last_rsu3 = *r;

        // LED on/off (간단 정책: active인 사고가 하나라도 있으면 on)
        if (sm->led) {
          bool any_active = false;
          for (int i = 0; i < n; i++) if (table[i].active) { any_active = true; break; }
          (void)led_set(sm->led, any_active);
        }
      }
      free(r);
    }
    else if (ev->type == EV_TIMER_2S_TICK) {
      // active 사고들에 대해 RSU-1 패킷을 만들어 무선 TX 큐로 push
      for (int i = 0; i < n; i++) {
        if (!table[i].active) continue;

        if (now > table[i].expire_ms) {
          table[i].active = false;
          continue;
        }

        uint8_t *pkt = (uint8_t*)calloc(1, 256);
        if (!pkt) continue;

        if (!packet_build_rsu1_wireless_payload(&table[i].last_rsu3, pkt)) {
          free(pkt);
          continue;
        }

        (void)bq_push(sm->to_air_q, pkt);
      }

      // 다음 tick 예약
      schedule_next_2s(sm->sched, sm->in_ev_q);

      // LED 상태 재평가(만료로 active가 줄었을 수 있음)
      if (sm->led) {
        bool any_active = false;
        for (int i = 0; i < n; i++) if (table[i].active) { any_active = true; break; }
        (void)led_set(sm->led, any_active);
      }
    }

    free(ev);
  }

  return NULL;
}

int state_manager_start(state_manager_t *sm,
                        const app_config_t *cfg,
                        bq_t *in_ev_q,
                        bq_t *to_tx_cmd_q,
                        bq_t *to_air_q,
                        scheduler_t *sched,
                        led_handle_t *led) {
  memset(sm, 0, sizeof(*sm));
  sm->cfg = cfg;
  sm->in_ev_q = in_ev_q;
  sm->to_tx_cmd_q = to_tx_cmd_q;
  sm->to_air_q = to_air_q;
  sm->sched = sched;
  sm->led = led;

  if (pthread_create(&sm->th, NULL, sm_thread, sm) != 0) return -1;
  return 0;
}

void state_manager_stop(state_manager_t *sm) {
  if (!sm) return;
  sm->running = false;
  pthread_join(sm->th, NULL);
}
