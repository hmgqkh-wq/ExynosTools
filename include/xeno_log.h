// include/xeno_log.h
// Thin compatibility header mapping project logging macros to the minimal logger.
// Ensures legacy code that expects XENO_LOG* and xeno_log_stream compiles and links.

#ifndef XENO_LOG_H_
#define XENO_LOG_H_

#include <stdarg.h>
#include "logging.h"

/* Map legacy macro names to current logger */
#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#define XENO_LOGI(...) logging_info(__VA_ARGS__)

/* Simple log stream object placeholder used by detect/app_profile.
   The original project used xeno_log_stream as a lightweight object for formatted output.
   Provide a minimal implementation and helper to maintain ABI. */

typedef struct xeno_log_stream {
    int level; /* unused but keeps structure shape */
} xeno_log_stream;

/* Initialize a stream (returns pointer to static object) */
static inline xeno_log_stream *xeno_log_stream_begin(void)
{
    static xeno_log_stream s = {0};
    return &s;
}

/* Write to stream with printf-style formatting */
static inline void xeno_log_stream_write(xeno_log_stream *s, const char *fmt, ...)
{
    (void)s;
    va_list ap;
    va_start(ap, fmt);
    logging_info(fmt, ap); /* logging_info is variadic; but we want printf-like behavior */
    va_end(ap);
}

/* Convenience function used by some sources */
static inline void xeno_log_stream_printf(xeno_log_stream *s, const char *fmt, ...)
{
    (void)s;
    va_list ap;
    va_start(ap, fmt);
    logging_info(fmt, ap);
    va_end(ap);
}

/* Provide a symbol name that some C files call directly (non-inline)
   so we also emit a weak definition in xeno_log_stream.c to satisfy the linker. */
extern void xeno_log_stream(const char *fmt, ...);

#endif // XENO_LOG_H_
