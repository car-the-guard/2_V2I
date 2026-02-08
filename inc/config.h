// app/config.h
#pragma once
#include <stdint.h>

typedef struct {
  uint32_t rsu_id;

  // UDP 수신
  uint16_t wl1_listen_port;     // 예: 30000 (네 환경에 맞게)
  const char *wl1_bind_ip;      // "0.0.0.0"

  // TCP (D3-G client -> Server PC)
  const char *server_ip;        // 예: "192.168.0.10"
  uint16_t server_port;         // 20615
  uint16_t local_port;          // 20905

  // GPIO (libgpiod)
  const char *gpiochip;         // 예: "gpiochip0"
  unsigned int led_line;        // 라인 번호
} app_config_t;

int load_default_config(app_config_t *cfg);
