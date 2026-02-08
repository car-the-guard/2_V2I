// common/queue.c
#include "queue.h"
#include <stdlib.h>
#include <string.h>

int bq_init(bq_t *q, int cap, q_full_policy_t policy) {
  memset(q, 0, sizeof(*q));
  q->buf = (void**)calloc((size_t)cap, sizeof(void*));
  if (!q->buf) return -1;
  q->cap = cap;
  q->policy = policy;
  pthread_mutex_init(&q->mtx, NULL);
  pthread_cond_init(&q->not_empty, NULL);
  pthread_cond_init(&q->not_full, NULL);
  return 0;
}

void bq_stop(bq_t *q) {
  pthread_mutex_lock(&q->mtx);
  q->stop = true;
  pthread_cond_broadcast(&q->not_empty);
  pthread_cond_broadcast(&q->not_full);
  pthread_mutex_unlock(&q->mtx);
}

void bq_destroy(bq_t *q) {
  if (!q) return;
  free(q->buf);
  pthread_mutex_destroy(&q->mtx);
  pthread_cond_destroy(&q->not_empty);
  pthread_cond_destroy(&q->not_full);
}

uint64_t bq_drop_count(bq_t *q) {
  pthread_mutex_lock(&q->mtx);
  uint64_t d = q->drop_cnt;
  pthread_mutex_unlock(&q->mtx);
  return d;
}

bool bq_push(bq_t *q, void *item) {
  pthread_mutex_lock(&q->mtx);

  while (!q->stop && q->size == q->cap && q->policy == Q_BLOCK) {
    pthread_cond_wait(&q->not_full, &q->mtx);
  }
  if (q->stop) { pthread_mutex_unlock(&q->mtx); return false; }

  if (q->size == q->cap) {
    if (q->policy == Q_DROP_TAIL) {
      q->drop_cnt++;
      pthread_mutex_unlock(&q->mtx);
      return false;
    } else if (q->policy == Q_DROP_HEAD) {
      q->drop_cnt++;
      q->head = (q->head + 1) % q->cap;
      q->size--;
    }
  }

  q->buf[q->tail] = item;
  q->tail = (q->tail + 1) % q->cap;
  q->size++;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->mtx);
  return true;
}

void* bq_pop(bq_t *q) {
  pthread_mutex_lock(&q->mtx);
  while (!q->stop && q->size == 0) {
    pthread_cond_wait(&q->not_empty, &q->mtx);
  }
  if (q->stop && q->size == 0) { pthread_mutex_unlock(&q->mtx); return NULL; }

  void *item = q->buf[q->head];
  q->buf[q->head] = NULL;
  q->head = (q->head + 1) % q->cap;
  q->size--;
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->mtx);
  return item;
}
