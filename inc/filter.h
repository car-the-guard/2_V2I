#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "types.h"

/*
 * filter.c는 light+heavy filter를 묶은 모듈.
 * - light: length/ttl/ver/msg_type 같은 빠른 컷
 * - heavy: 담당영역/방향/심각도 등 상대적으로 무거운 판정
 */

typedef struct {
  uint32_t rsu_id;
  // TODO: rsu lat/lon, 직사각 담당영역 파라미터, direction 등 필요 시 추가
} filter_ctx_t;

void filter_init(filter_ctx_t *ctx, uint32_t rsu_id);

/*
 * light + heavy 모두 통과하면 true.
 * out_dist_m: (필요시) 사고-현재RSU 거리 산출 (지금은 스텁)
 */
bool filter_pass_all(const filter_ctx_t *ctx, const wl1_msg_t *m, uint32_t *out_dist_m);
