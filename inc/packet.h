#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "types.h"

// WL-1'(Payload) -> RSU-2'(Payload)
bool packet_wl1_to_rsu2(const wl1_payload_t *wl1, uint32_t rsu_id, 
                        uint32_t dist_m, rsu2_payload_t *out);

// RSU-3'(Payload) -> WL-1'(Payload, 즉 RSU-1')
bool packet_rsu3_to_wl1(const rsu3_payload_t *rsu3, wl1_payload_t *out);