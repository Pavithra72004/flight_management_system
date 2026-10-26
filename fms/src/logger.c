#define _POSIX_C_SOURCE 200809L
#include "fms.h"
#include <stdarg.h>

/* The logger writes lines like:
 * [2026-10-18 10:32:11.482] [INFO] [booking] booking 1000 confirmed by user 2
 * RULE: never log passwords, hashes or full ID numbers. */
static FILE           *g_log = NULL;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char *path)
{
    pthread_mutex_lock(&g_log_lock);
    if (g_log == NULL && path != NULL) {
        g_log = fopen(path, "a");              /* may fail: logging is then skipped */
    }
    pthread_mutex_unlock(&g_log_lock);
}

void log_close(void)
{
    pthread_mutex_lock(&g_log_lock);
    if (g_log != NULL) {
        (void)fclose(g_log);
        g_log = NULL;
    }
    pthread_mutex_unlock(&g_log_lock);
}

void log_write(const char *level, const char *module, const char *fmt, ...)
{
    pthread_mutex_lock(&g_log_lock);
    if (g_log != NULL) {
        struct timespec ts;
        struct tm       tmv;
        char            stamp[32];
        va_list         ap;

        clock_gettime(CLOCK_REALTIME, &ts);
        localtime_r(&ts.tv_sec, &tmv);
        strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tmv);
        fprintf(g_log, "[%s.%03ld] [%s] [%s] ", stamp, ts.tv_nsec / 1000000L, level, module);
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fputc('\n', g_log);
        fflush(g_log);
    }
    pthread_mutex_unlock(&g_log_lock);
}
