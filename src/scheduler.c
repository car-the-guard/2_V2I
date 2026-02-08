// scheduler.c
#include "scheduler.h"
#include "timeutil.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void heap_swap(timer_item_t *a, timer_item_t *b){ timer_item_t t=*a;*a=*b;*b=t; }
static void heap_up(timer_item_t *h, size_t idx) {
  while (idx>0) {
    size_t p=(idx-1)/2;
    if (h[p].due_ms <= h[idx].due_ms) break;
    heap_swap(&h[p], &h[idx]);
    idx=p;
  }
}
static void heap_down(timer_item_t *h, size_t n, size_t idx) {
  for (;;) {
    size_t l=idx*2+1, r=idx*2+2, m=idx;
    if (l<n && h[l].due_ms < h[m].due_ms) m=l;
    if (r<n && h[r].due_ms < h[m].due_ms) m=r;
    if (m==idx) break;
    heap_swap(&h[m], &h[idx]);
    idx=m;
  }
}

int scheduler_init(scheduler_t *s, size_t cap) {
  memset(s, 0, sizeof(*s));
  s->heap = (timer_item_t*)calloc(cap, sizeof(timer_item_t));
  if (!s->heap) return -1;
  s->cap = cap;
  pthread_mutex_init(&s->mtx, NULL);
  pthread_cond_init(&s->cv, NULL);
  return 0;
}

void scheduler_stop(scheduler_t *s) {
  pthread_mutex_lock(&s->mtx);
  s->stop = true;
  pthread_cond_broadcast(&s->cv);
  pthread_mutex_unlock(&s->mtx);
}

void scheduler_destroy(scheduler_t *s) {
  free(s->heap);
  pthread_mutex_destroy(&s->mtx);
  pthread_cond_destroy(&s->cv);
}

bool scheduler_add(scheduler_t *s, uint64_t due_ms, timer_cb_t cb, void *arg) {
  pthread_mutex_lock(&s->mtx);
  if (s->size == s->cap) { pthread_mutex_unlock(&s->mtx); return false; }

  bool wake = (s->size == 0) || (due_ms < s->heap[0].due_ms);
  s->heap[s->size] = (timer_item_t){ .due_ms=due_ms, .cb=cb, .arg=arg };
  heap_up(s->heap, s->size);
  s->size++;
  if (wake) pthread_cond_signal(&s->cv);
  pthread_mutex_unlock(&s->mtx);
  return true;
}

void* scheduler_thread(void *arg) {
  scheduler_t *s = (scheduler_t*)arg;

  for (;;) {
    pthread_mutex_lock(&s->mtx);
    while (!s->stop && s->size == 0) {
      pthread_cond_wait(&s->cv, &s->mtx);
    }
    if (s->stop) { pthread_mutex_unlock(&s->mtx); break; }

    uint64_t now = now_ms_monotonic();
    timer_item_t top = s->heap[0];

    if (top.due_ms > now) {
      uint64_t wait_ms = top.due_ms - now;
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      uint64_t nsec = (uint64_t)ts.tv_nsec + wait_ms * 1000000ull;
      ts.tv_sec += (time_t)(nsec / 1000000000ull);
      ts.tv_nsec = (long)(nsec % 1000000000ull);
      pthread_cond_timedwait(&s->cv, &s->mtx, &ts);
      pthread_mutex_unlock(&s->mtx);
      continue;
    }

    s->heap[0] = s->heap[s->size - 1];
    s->size--;
    heap_down(s->heap, s->size, 0);
    pthread_mutex_unlock(&s->mtx);

    if (top.cb) top.cb(top.arg);
  }
  return NULL;
}
