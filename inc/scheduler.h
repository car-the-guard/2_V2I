// common/scheduler.h
#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*timer_cb_t)(void *arg);

typedef struct {
  uint64_t due_ms;
  timer_cb_t cb;
  void *arg;
} timer_item_t;

typedef struct {
  timer_item_t *heap;
  size_t size, cap;
  pthread_mutex_t mtx;
  pthread_cond_t cv;
  bool stop;
} scheduler_t;

int  scheduler_init(scheduler_t *s, size_t cap);
void scheduler_stop(scheduler_t *s);
void scheduler_destroy(scheduler_t *s);

bool scheduler_add(scheduler_t *s, uint64_t due_ms, timer_cb_t cb, void *arg);
void* scheduler_thread(void *arg);
