#include "pipeline.h"
#include "log.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h> 

static volatile int g_stop = 0;
static void on_sig(int s){ (void)s; g_stop = 1; }

// 가짜 패킷을 만들어 큐에 넣는 헬퍼 함수
void send_fake_packet(pipeline_t *p, uint32_t rsu_id, uint64_t acc_id) {
    tx_cmd_wired_t *cmd = (tx_cmd_wired_t*)calloc(1, sizeof(tx_cmd_wired_t));
    cmd->rsu2p = (rsu2_payload_t*)calloc(1, sizeof(rsu2_payload_t));
  
    // RSU ID & 좌표 (Big Endian)
    cmd->rsu2p->rsu_id = htonl(rsu_id);
    cmd->rsu2p->accident.lat = htonl(37566500); 
    cmd->rsu2p->accident.lon = htonl(126978000); 

    // [핵심] 사고 ID 설정
    cmd->rsu2p->accident.accident_id = acc_id; 

    // 기타 정보
    cmd->rsu2p->accident.severity = 2; 
    cmd->rsu2p->accident.lane = 1;     
    cmd->rsu2p->rsu_info.distance = htons(50); 
    cmd->rsu2p->rsu_info.acc_flag = htons(0x0000); // ON
    cmd->rsu2p->rsu_info.rsu_rx_time = 1000; 

    // RSU 내부 파이프라인 태우기 (State Manager로 전달)
    // 주의: 원래는 Wireless -> SM 순서지만, 
    // 테스트를 위해 SM이 받는 큐(in_ev_q)가 아니라 
    // SM이 처리하는 로직을 시뮬레이션하기 위해
    // 여기서는 'Wired Client'로 바로 보내지 않고,
    // 'State Manager'가 처리하도록 EV_WL1_RX 이벤트를 만들어 던져야
    // 중복 필터링 로직을 거칩니다.
    
    // 하지만 main.c에서는 SM 내부 큐(p->Q_sm_events)에 접근 가능하므로
    // 직접 이벤트를 만들어 넣겠습니다.
    
    sm_event_t *ev = (sm_event_t*)calloc(1, sizeof(sm_event_t));
    ev->type = EV_WL1_RX;
    ev->u.rsu2p = cmd->rsu2p; // Payload만 전달 (cmd 껍데기는 필요 없음)
    
    free(cmd); // cmd 껍데기는 해제, payload는 이벤트가 가짐
    
    bq_push(&p->Q_sm_events, ev);
}

int main(void) {
  signal(SIGINT, on_sig);
  signal(SIGTERM, on_sig);

  pipeline_t p;
  if (pipeline_start(&p) != 0) {
    LOGE("pipeline_start failed");
    return 1;
  }

  // ============================================================
  // [중복 전송 테스트]
  // ============================================================
  
  // 1. 첫 번째 패킷 전송 (ID: 0x9999)
  LOGI("TEST: Sending 1st Packet (ID: 0x9999) ...");
  send_fake_packet(&p, 200, 0x9999);
  
  // 1초 대기 (사람 눈으로 확인)
  sleep(1);
  
  // 2. 두 번째 패킷 전송 (ID: 0x9999) - 똑같은 ID!
  LOGI("TEST: Sending 2nd Packet (ID: 0x9999) - Should be IGNORED locally");
  send_fake_packet(&p, 200, 0x9999);

  // ============================================================

  while (!g_stop) sleep(1);

  pipeline_stop(&p);
  return 0;
}