#include "filter.h"

static bool light_pass(const wl1_msg_t *m) {
  if (!m) return false;

  // 1) length==256 : wireless_rx에서 recv 길이로 컷 (여기선 raw 고정이라 생략)
  // 2) msg_type==RSU면 drop (예시로 1을 RSU로 둠)
  if (m->msg_type == 1) return false;

  // 3) ttl==3 (RSU는 원본만 받음)
  if (m->ttl != 3) return false;

  // 4) header version 체크 (예시로 ver==1)
  if (m->ver != 1) return false;

  return true;
}

static bool heavy_pass(const filter_ctx_t *ctx, const wl1_msg_t *m, uint32_t *out_dist_m) {
  (void)ctx;
  if (!m) return false;

  // severity >= 2
  if (m->severity < 2) return false;

  // TODO: 직사각 담당영역 판정 + direction 상/하행 일치 판정
  // 지금은 스텁으로 통과 처리
  if (out_dist_m) *out_dist_m = 120;

  return true;
}

void filter_init(filter_ctx_t *ctx, uint32_t rsu_id) {
  if (!ctx) return;
  ctx->rsu_id = rsu_id;
}

bool filter_pass_all(const void *raw_pkt, uint32_t rsu_id, uint32_t *out_dist_m) {
    const wl1_packet_t *pkt = (const wl1_packet_t*)raw_pkt;

    // 1. Light Filter
    if (pkt->payload.header.msg_type == 0x01) return false; // RSU가 보낸건 Drop
    if (pkt->payload.header.ttl != 1) return false;         // TTL != 1 Drop (이미지엔 1 고정)

    // 2. Heavy Filter (Severity)
    if (pkt->payload.accident.severity < 2) return false;

    // 거리 계산 로직 (Stub)
    if (out_dist_m) *out_dist_m = 150; // 예시 값

    return true;
}