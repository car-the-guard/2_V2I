// io/led.c
#include "led.h"
#include "log.h"
#include <gpiod.h>
#include <stdlib.h>
#include <string.h>

struct led_handle {
  struct gpiod_chip *chip;
  struct gpiod_line *line;
};

led_handle_t* led_open(const char *gpiochip, unsigned int line) {
  led_handle_t *h = (led_handle_t*)calloc(1, sizeof(*h));
  if (!h) return NULL;

  if (!gpiochip) {
    LOGE("led_open: gpiochip is NULL");
    free(h);
    return NULL;
  }

  // 1) by_name: "gpiochip0" 같은 커널 디바이스 이름
  h->chip = gpiod_chip_open_by_name(gpiochip);

  // 2) by_label: "gpioa" 같은 label (gpiodetect의 [] 안 값)
  if (!h->chip) {
    h->chip = gpiod_chip_open_by_label(gpiochip);
  }

  // 3) by_path: "/dev/gpiochip0" 같은 절대 경로
  if (!h->chip && gpiochip[0] == '/') {
    h->chip = gpiod_chip_open(gpiochip);
  }

  if (!h->chip) {
    LOGE("gpiod_chip_open failed (name/label/path): %s", gpiochip);
    free(h);
    return NULL;
  }

  h->line = gpiod_chip_get_line(h->chip, (unsigned int)line);
  if (!h->line) {
    LOGE("gpiod_chip_get_line failed (line=%u)", line);
    gpiod_chip_close(h->chip);
    free(h);
    return NULL;
  }

  // output으로 요청 (초기값 0)
  if (gpiod_line_request_output(h->line, "rsu-led", 0) < 0) {
    LOGE("gpiod_line_request_output failed (line=%u)", line);
    gpiod_chip_close(h->chip);
    free(h);
    return NULL;
  }

  LOGI("LED ready: chip=%s line=%u", gpiochip, line);
  return h;
}

void led_close(led_handle_t *h) {
  if (!h) return;
  // gpiod_line_release(h->line); // (버전에 따라 필요)
  if (h->chip) gpiod_chip_close(h->chip);
  free(h);
}

bool led_set(led_handle_t *h, bool on) {
  if (!h || !h->line) return false;
  if (gpiod_line_set_value(h->line, on ? 1 : 0) < 0) {
    LOGW("gpiod_line_set_value failed");
    return false;
  }
  
  LOGI("LED set: %d", on);
  return true;
}
