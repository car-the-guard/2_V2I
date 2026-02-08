#pragma once
#include <stdint.h>

typedef struct {
  uint8_t raw[256];
  uint64_t rx_ms;
  uint32_t accident_id;
  uint32_t sender_id;
  uint8_t msg_type;
  uint8_t ttl;
  uint8_t ver;
  uint8_t severity;
  uint8_t direction;
  double lat, lon;
} wl1_msg_t;

typedef struct {
  uint32_t rsu_id;
  uint32_t accident_id;
  uint64_t rx_ms;
  uint32_t dist_m;
  uint8_t onoff;
  uint8_t severity;
  uint8_t direction;
  double lat, lon;
} rsu2p_msg_t;

typedef struct {
  uint32_t accident_id;
  uint8_t cmd_onoff; // 1=on, 0=off
  uint8_t direction;
  uint8_t severity;
  double lat, lon;
  uint64_t rx_ms;
} rsu3p_msg_t;

typedef enum {
  EV_FROM_WL1_RSU2P = 1,
  EV_FROM_SERVER_RSU3P,
  EV_TIMER_2S_TICK,
  EV_ACK_OK,
  EV_ACK_TIMEOUT
} ev_type_t;

typedef struct {
  ev_type_t type;
  union {
    rsu2p_msg_t *rsu2p;
    rsu3p_msg_t *rsu3p;
    uint32_t ack_msg_id;
  } u;
} sm_event_t;

typedef struct {
  // TxManager로 내려가는 명령(서버 전송)
  rsu2p_msg_t *rsu2p;
  uint32_t msg_id;
} tx_cmd_t;

typedef struct {
  // TCP RX에서 올라오는 ACK 이벤트(혹은 RSU-3 패킷)
  uint32_t msg_id;
} ack_msg_t;
