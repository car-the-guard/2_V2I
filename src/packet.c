#include "packet.h"
#include <string.h>
#include "timeutil.h" // now_ms_monotonic() 구현 가정

bool packet_wl1_to_rsu2(const wl1_payload_t *wl1, uint32_t rsu_id, 
                        uint32_t dist_m, rsu2_payload_t *out) {
    if (!wl1 || !out) return false;
    memset(out, 0, sizeof(*out));

    out->rsu_id = rsu_id;
    // 사고 정보 복사 (구조 동일)
    memcpy(&out->accident, &wl1->accident, sizeof(acc_info_t));

    out->rsu_info.distance = (uint16_t)dist_m;
    out->rsu_info.acc_flag = 0xFFFF; // 수신된 사고는 Active로 간주
    out->rsu_info.rsu_rx_time = now_ms_monotonic();

    return true;
}

bool packet_rsu3_to_wl1(const rsu3_payload_t *rsu3, wl1_payload_t *out) {
    if (!rsu3 || !out) return false;
    memset(out, 0, sizeof(*out));

    out->header.version = 1;
    out->header.msg_type = 0x01; // RSU -> Vehicle
    out->header.ttl = 1;

    out->sender.sender_id = rsu3->rsu_id;
    out->sender.send_time = now_ms_monotonic();
    
    // RSU 위치는 별도 Config에서 가져와야 하나, 여기서는 0으로 둠 (혹은 인자로 수신)
    out->sender.lat = 0; 
    out->sender.lon = 0; 
    out->sender.alt = 0;

    // 해제 신호 처리 (acc_flag가 0이면 해제 0xFF)
    if (rsu3->server_info.acc_flag == 0) {
        out->sender.reserved[0] = 0xFF;
    } else {
        out->sender.reserved[0] = 0x00;
    }

    memcpy(&out->accident, &rsu3->accident, sizeof(acc_info_t));
    return true;
}