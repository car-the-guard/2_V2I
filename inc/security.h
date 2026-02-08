#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "types.h"

// 무선: RX Strip (Packet -> Payload)
bool sec_wireless_rx_strip(const wl1_packet_t *pkt, wl1_payload_t *out_payload);

// 무선: TX Wrap (Payload -> Packet)
bool sec_wireless_tx_wrap(const wl1_payload_t *in_payload, wl1_packet_t *out_pkt);

// 유선: RX Strip (RSU-3 Packet -> Payload)
bool sec_wired_rx_strip(const rsu3_packet_t *pkt, rsu3_payload_t *out_payload);

// 유선: TX Wrap (RSU-2 Payload -> Packet)
bool sec_wired_tx_wrap(const rsu2_payload_t *in_payload, rsu2_packet_t *out_pkt);