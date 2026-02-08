#include "packet.h"
#include <string.h>

bool packet_build_rsu2p_from_wl1(uint32_t rsu_id,
                                const wl1_msg_t *m,
                                uint32_t dist_m,
                                rsu2p_msg_t *out) {
  if (!m || !out) return false;

  memset(out, 0, sizeof(*out));
  out->rsu_id = rsu_id;
  out->accident_id = m->accident_id;
  out->rx_ms = m->rx_ms;
  out->dist_m = dist_m;
  out->onoff = 1;
  out->severity = m->severity;
  out->direction = m->direction;
  out->lat = m->lat;
  out->lon = m->lon;

  return true;
}

bool packet_build_rsu1_wireless_payload(const rsu3p_msg_t *rsu3p,
                                        uint8_t out256[256]) {
  if (!rsu3p || !out256) return false;

  memset(out256, 0, 256);

  // TODO: RSU-1 실제 포맷 정의에 맞춰 헤더/센더/사고정보 구성
  // 스텁: accident_id + cmd_onoff만 넣음
  memcpy(out256, &rsu3p->accident_id, sizeof(rsu3p->accident_id));
  out256[4] = rsu3p->cmd_onoff;

  return true;
}
