#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "types.h"

/*
 * security.c는 sec_wireless + sec_wired를 묶은 모듈.
 * 지금은 스텁(그대로 통과)이고, 나중에 실제 서명검증/부착 구현.
 */

// 무선: 서명 검증 후 보안부 제거(또는 strip)된 데이터로 out_stripped 채움
bool security_wireless_verify_and_strip(const wl1_msg_t *in,
                                        wl1_msg_t *out_stripped);

// 유선: wrap/unwrap (보안헤더/서명 부착, 검증/제거)
bool security_wired_wrap(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t *out_len, size_t out_cap);

bool security_wired_unwrap(const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len, size_t out_cap);
