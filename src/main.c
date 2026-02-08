#include "pipeline.h"
#include "log.h"
#include <signal.h>
#include <unistd.h>

static volatile int g_stop = 0;
static void on_sig(int s){ (void)s; g_stop = 1; }

int main(void) {
  signal(SIGINT, on_sig);
  signal(SIGTERM, on_sig);

  pipeline_t p;
  if (pipeline_start(&p) != 0) {
    LOGE("pipeline_start failed");
    return 1;
  }

  while (!g_stop) sleep(1);

  pipeline_stop(&p);
  return 0;
}
