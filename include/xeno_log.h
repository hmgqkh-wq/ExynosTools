// include/xeno_log.h
// Minimal logging header used across the project.
// - Declares logging_info / logging_error used by many modules
// - Declares XENO_LOG* macros used in existing sources
// - Safe, compile-time friendly for both C and C++ consumers

#ifndef XENO_LOG_H
#define XENO_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

/* Basic logging function signatures used by the repo */
void logging_info(const char *fmt, ...);
void logging_warn(const char *fmt, ...);
void logging_error(const char *fmt, ...);
void logging_debug(const char *fmt, ...);

/* Simple macro wrappers matching symbols used in source files */
#define XENO_LOGI(...) logging_info(__VA_ARGS__)
#define XENO_LOGW(...) logging_warn(__VA_ARGS__)
#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#define XENO_LOGD(...) logging_debug(__VA_ARGS__)

/* Default inline implementations when no logging.c is provided.
   If you already have src/logging.c in the repo, these will be ignored at link time.
   These functions are lightweight and safe for CI to compile if logging.c is missing. */

#ifndef XENO_LOG_IMPLEMENTATION_PRESENT

static inline void logging_vprintf(const char *prefix, const char *fmt, va_list va)
{
    if (!fmt) return;
    if (prefix) fprintf(stderr, "%s: ", prefix);
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
}

static inline void logging_info(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); logging_vprintf("INFO", fmt, va); va_end(va);
}
static inline void logging_warn(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); logging_vprintf("WARN", fmt, va); va_end(va);
}
static inline void logging_error(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); logging_vprintf("ERROR", fmt, va); va_end(va);
}
static inline void logging_debug(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); logging_vprintf("DEBUG", fmt, va); va_end(va);
}

#endif /* XENO_LOG_IMPLEMENTATION_PRESENT */

#ifdef __cplusplus
}
#endif

#endif /* XENO_LOG_H */
