#include "debug.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

log_level_t g_log_level = LOG_DEBUG; // DEBUG 레벨까지 모두 출력

void debug_init() {
    // 인터넷 연결 시 시간 동기화 (필요 없으면 주석 처리)
    // system("sudo ntpdate -u pool.ntp.org"); 
    
    setenv("TZ", "KST-9", 1);
    tzset();
    printf("[DEBUG] System Initialized (KST)\n");
}

void debug_log(const char *level, const char *file, int line, const char *fmt, ...) {
    char time_buf[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    struct tm *tm = localtime(&tv.tv_sec);
    long millis = tv.tv_usec / 1000;
    // long micros = tv.tv_usec % 1000; // 마이크로초 단위까지 확인하고 싶다면

    // [HH:MM:SS.ms] 형식
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm);
    
    // 쓰레드 안전성을 위해 printf 한 번에 호출 권장
    printf("[%s.%03ld][%s][%s:%d] ", time_buf, millis, level, file, line);

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
}