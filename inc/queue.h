// common/queue.h
#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  Q_BLOCK = 0,
  Q_DROP_TAIL,
  Q_DROP_HEAD
} q_full_policy_t;

typedef struct {
  void **buf;
  int cap, head, tail, size;
  q_full_policy_t policy;
  pthread_mutex_t mtx;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
  uint64_t drop_cnt;
  bool stop;
} bq_t;

int  bq_init(bq_t *q, int cap, q_full_policy_t policy);
void bq_stop(bq_t *q);
void bq_destroy(bq_t *q);
bool bq_push(bq_t *q, void *item);
void* bq_pop(bq_t *q);
uint64_t bq_drop_count(bq_t *q);
