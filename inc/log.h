#pragma once
#include <stdio.h>

#define LOGI(fmt, ...) fprintf(stdout, "[I] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) fprintf(stdout, "[W] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
