#include "state_manager.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "types.h"
#include "timeutil.h"
#include "packet.h"
#include "security.h" 

// ---- 사고 테이블(간단 버전) ----
typedef struct {
  uint64_t accident_id;     
  bool active;
  uint64_t expire_ms;
  rsu3_payload_t last_rsu3; 
} acc_ent_t;

// ---- 2초 tick 이벤트를 SM 큐로 넣는 타이머 콜백 ----
static void post_tick_event(void *arg) {
  bq_t *q = (bq_t*)arg;
  sm_event_t *ev = (sm_event_t*)calloc(1, sizeof(*ev));
  if (!ev) return;
  ev->type = EV_TIMER_TICK;
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

    // 1. [WL-1 수신] -> 중복 검사 -> (통과 시) 서버 전송
    if (ev->type == EV_WL1_RX) {
      rsu2_payload_t *p = ev->u.rsu2p; // Packet Convert에서 이미 변환된 Payload

      // (1) 테이블 검색
      int idx = -1;
      for (int i = 0; i < n; i++) {
        if (table[i].accident_id == p->accident.accident_id) { 
            idx = i; 
            break; 
        }
      }

      // (2) 이미 알고 있고, 만료되지 않은(Active) 사고라면 -> 서버 전송 건너뜀 (DROP)
      if (idx >= 0 && table[idx].active && now < table[idx].expire_ms) {
          // LOGI("Duplicate accident report ignored (ID: %llx)", (long long)p->accident.accident_id);
          free(p); // 메모리 해제
      } 
      // (3) 새로운 사고라면 -> 테이블 등록 & 서버 전송
      else {
          // 테이블 공간이 남으면 등록
          if (idx < 0 && n < 256) {
              idx = n++;
              table[idx].accident_id = p->accident.accident_id;
          }
          
          if (idx >= 0) {
              table[idx].active = true;
              table[idx].expire_ms = now + 15000; // 15초간 중복 방지
              // last_rsu3는 아직 없음 (서버 응답 대기)
          }

          // 서버 전송 큐로 전달
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
    // 2. [서버(RSU-3) 수신] -> RSU-3' -> 사고 테이블 갱신
    else if (ev->type == EV_RSU3_RX) {
      rsu3_payload_t *r = ev->u.rsu3p;

      int idx = -1;
      for (int i = 0; i < n; i++) {
        if (table[i].accident_id == r->accident.accident_id) { idx = i; break; }
      }
      
      // 서버가 먼저 알려준 경우에도 등록
      if (idx < 0 && n < 256) {
        idx = n++;
        table[idx].accident_id = r->accident.accident_id;
      }

      if (idx >= 0) {
        table[idx].active = (r->server_info.acc_flag != 0);
        table[idx].expire_ms = now + 15000; 
        table[idx].last_rsu3 = *r;          

        if (sm->led) {
          bool any_active = false;
          for (int i = 0; i < n; i++) if (table[i].active) { any_active = true; break; }
          (void)led_set(sm->led, any_active);
        }
      }
      free(r);
    }
    // 3. [2초 타이머] -> Active 사고 무선 송신 (WL-1/RSU-1)
    else if (ev->type == EV_TIMER_TICK) {
      bool any_active = false;
      for (int i = 0; i < n; i++) {
        if (!table[i].active) continue;

        if (now > table[i].expire_ms) {
          table[i].active = false;
          continue;
        }
        any_active = true;

        // RSU-3 정보가 있어야 전파 가능 (서버 승인 패킷 기반 조립)
        if (table[i].last_rsu3.rsu_id == 0) continue; 

        // 3-1. RSU-3' -> WL-1' (Payload 변환)
        wl1_payload_t wl1p;
        if (!packet_rsu3_to_wl1(&table[i].last_rsu3, &wl1p)) {
          continue;
        }

        // 3-2. WL-1' -> WL-1 Packet (보안 Wrap)
        wl1_packet_t *pkt = (wl1_packet_t*)calloc(1, sizeof(wl1_packet_t));
        if (!pkt) continue;

        if (!sec_wireless_tx_wrap(&wl1p, pkt)) {
            free(pkt);
            continue;
        }

        (void)bq_push(sm->to_air_q, pkt);
      }

      // 다음 tick 예약
      schedule_next_2s(sm->sched, sm->in_ev_q);

      // LED 상태 재평가
      if (sm->led) {
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