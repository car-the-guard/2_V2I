// io/led.h
#pragma once
#include <stdbool.h>

typedef struct led_handle led_handle_t;

led_handle_t* led_open(const char *gpiochip, unsigned int line);
void led_close(led_handle_t *h);

bool led_set(led_handle_t *h, bool on);
