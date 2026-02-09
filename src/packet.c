#include "packet.h"
#include <string.h>
#include "timeutil.h" // now_ms_monotonic() 구현 가정
#include <arpa/inet.h>

bool packet_wl1_to_rsu2(const wl1_payload_t *wl1, uint32_t rsu_id, 
                        uint32_t dist_m, rsu2_payload_t *out) {
    if (!wl1 || !out) return false;
    memset(out, 0, sizeof(*out));

    // [설계 반영] 서버로 보낼 때는 Big Endian으로 변환!
    out->rsu_id = htonl(rsu_id);
    
    // 사고 정보 복사 (변환 필요)
    // 주의: wl1(수신된 것)은 이미 Big Endian일 수도 있고 아닐 수도 있음.
    // 보통 무선 패킷도 Big Endian이 표준이지만, 여기선 그대로 왔다고 가정하고
    // 서버 규격에 맞춰 다시 htonl을 해줍니다.
    
    out->accident.accident_id = wl1->accident.accident_id; // 64bit는 그대로 (혹은 별도 처리)
    out->accident.lat = htonl(wl1->accident.lat);
    out->accident.lon = htonl(wl1->accident.lon);
    out->accident.alt = htonl(wl1->accident.alt);
    out->accident.direction = htons(wl1->accident.direction);
    
    out->accident.severity = wl1->accident.severity; // 1byte는 변환 불필요
    out->accident.lane = wl1->accident.lane;         // 1byte는 변환 불필요

    // RSU Info 설정
    out->rsu_info.distance = htons((uint16_t)dist_m);
    out->rsu_info.acc_flag = htons(0x0000); // ON 상태
    out->rsu_info.rsu_rx_time = now_ms_monotonic(); // 64bit

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