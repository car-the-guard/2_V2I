#include "security.h"
#include <string.h>

bool sec_wireless_rx_strip(const wl1_packet_t *pkt, wl1_payload_t *out_payload) {
    if (!pkt || !out_payload) return false;
    // 서명 검증 로직(생략) -> Pass
    memcpy(out_payload, &pkt->payload, sizeof(wl1_payload_t));
    return true;
}

bool sec_wireless_tx_wrap(const wl1_payload_t *in_payload, wl1_packet_t *out_pkt) {
    if (!in_payload || !out_pkt) return false;
    memcpy(&out_pkt->payload, in_payload, sizeof(wl1_payload_t));
    // 더미 서명 채우기
    memset(out_pkt->security, 0xEE, WL_SEC_SIZE);
    return true;
}

bool sec_wired_rx_strip(const rsu3_packet_t *pkt, rsu3_payload_t *out_payload) {
    if (!pkt || !out_payload) return false;
    // 토큰 검증 로직(생략) -> Pass
    memcpy(out_payload, &pkt->payload, sizeof(rsu3_payload_t));
    return true;
}

bool sec_wired_tx_wrap(const rsu2_payload_t *in_payload, rsu2_packet_t *out_pkt) {
    if (!in_payload || !out_pkt) return false;
    memcpy(&out_pkt->payload, in_payload, sizeof(rsu2_payload_t));
    
    // Token 채우기 (이미지대로 RSU ID를 사용)
    memset(out_pkt->token, 0, WIRED_TOKEN_SIZE);
    memcpy(out_pkt->token, &in_payload->rsu_id, sizeof(in_payload->rsu_id));
    return true;
}