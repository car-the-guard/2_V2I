#include "filter.h"
#include "types.h"

static bool light_pass(const wl1_packet_t *pkt) {
  if (!pkt) return false;

  // 1) msg_type==0x01(RSU)면 drop
  if (pkt->payload.header.msg_type == 0x01) return false;

  // 2) ttl!=1 이면 drop
  if (pkt->payload.header.ttl != 3) return false;

  // 3) header version 체크 (예: 1)
  if (pkt->payload.header.version != 1) return false;

  return true;
}

static bool heavy_pass(const wl1_packet_t *pkt, uint32_t *out_dist_m) {
  if (!pkt) return false;

  // severity < 2 drop
  if (pkt->payload.accident.severity < 2) return false;

  // TODO: 직사각 담당영역 판정 + direction 상/하행 일치 판정
  // 지금은 스텁으로 통과 처리
  if (out_dist_m) *out_dist_m = 120; // 예시 거리

  return true;
}

void filter_init(filter_ctx_t *ctx, uint32_t rsu_id) {
  if (!ctx) return;
  ctx->rsu_id = rsu_id;
}

bool filter_pass_all(const void *raw_pkt, uint32_t rsu_id, uint32_t *out_dist_m) {
    (void)rsu_id; // 경고 방지
    const wl1_packet_t *pkt = (const wl1_packet_t*)raw_pkt;

    if (!light_pass(pkt)) return false;
    if (!heavy_pass(pkt, out_dist_m)) return false;

    return true;
}