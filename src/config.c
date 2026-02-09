// app/config.c
#include "config.h"
#include <string.h>

int load_default_config(app_config_t *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->rsu_id = 200; // RSU ID (원하는 대로 변경 가능)

  cfg->wl1_listen_port = 30000;
  cfg->wl1_bind_ip = "0.0.0.0";

  // 서버(PC)의 IP 주소
  cfg->server_ip = "192.168.137.1"; 
  
  // 포트는 서버 코드와 일치해야 함 (기본 20615)
  cfg->server_port = 20615;
  cfg->local_port = 20905;  // RSU가 사용할 포트

  cfg->gpiochip = "gpioa";
  cfg->led_line = 17;
  return 0;
}