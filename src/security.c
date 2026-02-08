#include "security.h"
#include <string.h>

bool security_wireless_verify_and_strip(const wl1_msg_t *in,
                                        wl1_msg_t *out_stripped) {
  if (!in || !out_stripped) return false;

  // TODO: 무선 서명 검증
  // TODO: 검증 통과 시 보안부(192B 등) 제거 후 필요한 필드만 유지
  // 스텁: 그대로 복사
  memcpy(out_stripped, in, sizeof(*out_stripped));
  return true;
}

bool security_wired_wrap(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t *out_len, size_t out_cap) {
  if (!in || !out || !out_len) return false;
  if (in_len > out_cap) return false;

  // TODO: 유선 보안 헤더/서명 부착
  memcpy(out, in, in_len);
  *out_len = in_len;
  return true;
}

bool security_wired_unwrap(const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t *out_len, size_t out_cap) {
  if (!in || !out || !out_len) return false;
  if (in_len > out_cap) return false;

  // TODO: 유선 보안 검증 + 보안부 제거
  memcpy(out, in, in_len);
  *out_len = in_len;
  return true;
}
