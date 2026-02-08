// app/config.c
#include "config.h"
#include <string.h>

int load_default_config(app_config_t *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->rsu_id = 200;

  cfg->wl1_listen_port = 30000;
  cfg->wl1_bind_ip = "0.0.0.0";

  cfg->server_ip = "127.0.0.1";
  cfg->server_port = 20615;
  cfg->local_port = 20905;

  cfg->gpiochip = "gpioa";
  cfg->led_line = 17; // TODO: 보드 실제 라인으로 변경
  return 0;
}