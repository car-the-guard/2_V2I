#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "types.h"

/*
 * packet.c는 packet_rx + packet_tx를 묶은 모듈.
 */

bool packet_build_rsu2p_from_wl1(uint32_t rsu_id,
                                const wl1_msg_t *m,
                                uint32_t dist_m,
                                rsu2p_msg_t *out);

bool packet_build_rsu1_wireless_payload(const rsu3p_msg_t *rsu3p,
                                        uint8_t out256[256]);
