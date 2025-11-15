// src/logging.c
#define XENO_LOG_IMPLEMENTATION_PRESENT
#include "xeno_log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static void xeno_log_vprint_impl(const char *tag, const char *fmt, va_list va)
{
    time_t t = time(NULL);
    struct tm tm;
#if defined(_MSC_VER)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ts[32] = {0};
    if (strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm) == 0) ts[0] = '\0';

    if (tag && tag[0]) fprintf(stderr, "%s [%s] ", ts, tag);
    else fprintf(stderr, "%s ", ts);

    if (fmt) vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void logging_info(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint_impl("INFO", fmt, va); va_end(va);
}
void logging_warn(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint_impl("WARN", fmt, va); va_end(va);
}
void logging_error(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint_impl("ERROR", fmt, va); va_end(va);
}
void logging_debug(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint_impl("DEBUG", fmt, va); va_end(va);
}
