// include/xeno_log.h
#ifndef XENO_LOG_H
#define XENO_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

/*
 Policy:
 - If you have a single translation-unit implementation (e.g., src/logging.c),
   define XENO_LOG_IMPLEMENTATION_PRESENT in that TU before including this header.
   That will expose non-static prototypes so other TUs link to the real functions.
 - Otherwise, this header provides static inline fallback implementations
   so compilation succeeds even if no logging.c exists.
*/

/* When a real implementation exists, expose non-static prototypes for linkage. */
#ifdef XENO_LOG_IMPLEMENTATION_PRESENT

void logging_info(const char *fmt, ...);
void logging_warn(const char *fmt, ...);
void logging_error(const char *fmt, ...);
void logging_debug(const char *fmt, ...);

#else /* fallback static-inline implementations (header-only) */

static inline void xeno_log_vprint(const char *tag, const char *fmt, va_list va)
{
    if (tag) fprintf(stderr, "%s: ", tag);
    if (fmt) vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
}

static inline void logging_info(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint("INFO", fmt, va); va_end(va);
}
static inline void logging_warn(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint("WARN", fmt, va); va_end(va);
}
static inline void logging_error(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint("ERROR", fmt, va); va_end(va);
}
static inline void logging_debug(const char *fmt, ...)
{
    va_list va; va_start(va, fmt); xeno_log_vprint("DEBUG", fmt, va); va_end(va);
}

#endif /* XENO_LOG_IMPLEMENTATION_PRESENT */

/* Macros used across source */
#define XENO_LOGI(...) logging_info(__VA_ARGS__)
#define XENO_LOGW(...) logging_warn(__VA_ARGS__)
#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#define XENO_LOGD(...) logging_debug(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* XENO_LOG_H */
