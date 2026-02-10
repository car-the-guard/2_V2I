#include "state_manager.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "log.h"
#include "types.h"
#include "timeutil.h"
#include "packet.h"
#include "security.h" 

// ---- 사고 테이블 ----
typedef struct {
  uint64_t accident_id;     
  bool active;
  uint64_t expire_ms;
  rsu3_payload_t last_rsu3; 
} acc_ent_t;

// ---- 2초 tick 이벤트 ----
static void post_tick_event(void *arg) {
  bq_t *q = (bq_t*)arg;
  sm_event_t *ev = (sm_event_t*)calloc(1, sizeof(*ev));
  if (!ev) return;
  ev->type = EV_TIMER_TICK;
  (void)bq_push(q, ev);
}

static void schedule_next_2s(scheduler_t *sched, bq_t *evq) {
  (void)scheduler_add(sched, now_ms_monotonic() + 2000, post_tick_event, evq);
}

static void* sm_thread(void *arg) {
  state_manager_t *sm = (state_manager_t*)arg;
  sm->running = true;

  acc_ent_t table[256];
  memset(table, 0, sizeof(table));
  int n = 0;

  schedule_next_2s(sm->sched, sm->in_ev_q);

  while (sm->running) {
    sm_event_t *ev = (sm_event_t*)bq_pop(sm->in_ev_q);
    if (!ev) break;

    uint64_t now = now_ms_monotonic();

    // 1. [WL-1 수신] 차량 사고 보고 -> LED 즉시 점등
    if (ev->type == EV_WL1_RX) {
      rsu2_payload_t *p = ev->u.rsu2p;

      // (1) 중복 검색
      int idx = -1;
      for (int i = 0; i < n; i++) {
        if (table[i].accident_id == p->accident.accident_id) { 
            idx = i; 
            break; 
        }
      }

      // (2) 이미 알고 있는 Active 사고 -> 무시
      if (idx >= 0 && table[idx].active) {
          free(p); 
      } 
      // (3) 새로운 사고 -> 등록 & LED ON & 서버 전송
      else {
          if (idx < 0 && n < 256) {
              idx = n++;
              table[idx].accident_id = p->accident.accident_id;
          }
          
          if (idx >= 0) {
              table[idx].active = true;
              table[idx].expire_ms = UINT64_MAX;
              
              // [핵심 수정] 사고 인지 즉시 LED 켜기!
              if (sm->led) {
                  led_set(sm->led, true);
                  LOGI("!! EMERGENCY !! Accident Detected -> LED ON");
              }
          }

          // 서버 전송
          tx_cmd_wired_t *cmd = (tx_cmd_wired_t*)calloc(1, sizeof(*cmd)); 
          if (cmd) {
            cmd->rsu2p = p; 
            (void)bq_push(sm->to_tx_cmd_q, cmd);
            LOGI("New accident reported to server (ID: %llx)", (long long)p->accident.accident_id);
          } else {
            free(p);
          }
      }
    }
    // 2. [서버(RSU-3) 수신] -> 상태 동기화
    else if (ev->type == EV_RSU3_RX) {
      rsu3_payload_t *r = ev->u.rsu3p;

      int idx = -1;
      for (int i = 0; i < n; i++) {
        if (table[i].accident_id == r->accident.accident_id) { idx = i; break; }
      }
      
      if (idx < 0 && n < 256) {
        idx = n++;
        table[idx].accident_id = r->accident.accident_id;
      }

      if (idx >= 0) {
        bool is_alarm_on = (r->server_info.acc_flag != 0);

        table[idx].active = is_alarm_on;
        table[idx].expire_ms = UINT64_MAX; 
        table[idx].last_rsu3 = *r;          

        // 서버 정보에 따라 LED 상태 갱신
        if (sm->led) {
          bool any_active = false;
          for (int i = 0; i < n; i++) if (table[i].active) { any_active = true; break; }
          (void)led_set(sm->led, any_active);
          LOGI("Server update -> LED %s", any_active ? "ON" : "OFF");
        }
      }
      free(r);
    }
    // 3. [2초 타이머] -> 주기적 전파 및 LED 상태 유지
    else if (ev->type == EV_TIMER_TICK) {
      
      // LOGI("[Alive] State Manager Tick..."); // 생존 확인용 (필요시 주석 해제)

      bool any_active = false;
      for (int i = 0; i < n; i++) {
        if (!table[i].active) continue;

        if (now > table[i].expire_ms) {
          table[i].active = false;
          LOGI("Accident ID %llx expired.", (long long)table[i].accident_id);
          continue;
        }
        any_active = true;

        if (table[i].last_rsu3.rsu_id == 0) continue; 

        wl1_payload_t wl1p;
        if (!packet_rsu3_to_wl1(&table[i].last_rsu3, &wl1p)) continue;

        wl1_packet_t *pkt = (wl1_packet_t*)calloc(1, sizeof(wl1_packet_t));
        if (!pkt) continue;

        if (!sec_wireless_tx_wrap(&wl1p, pkt)) {
            free(pkt);
            continue;
        }
        (void)bq_push(sm->to_air_q, pkt);
      }

      schedule_next_2s(sm->sched, sm->in_ev_q);
      
      // 주기적으로 LED 상태 재확인 (혹시 꺼졌을까봐)
      if (sm->led) (void)led_set(sm->led, any_active);
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