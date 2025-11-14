// include/xeno_log.h
// Thin compatibility header mapping legacy logging names to the current logging API.

#ifndef XENO_LOG_H_
#define XENO_LOG_H_

#include <stdarg.h>
#include "logging.h"

/* Legacy macros mapped to current logging implementation */
#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#define XENO_LOGI(...) logging_info(__VA_ARGS__)

/* Minimal xeno_log_stream type used by older modules */
typedef struct xeno_log_stream {
    int level;
} xeno_log_stream;

/* Inline helpers used by source that expect a stream API */
static inline xeno_log_stream *xeno_log_stream_begin(void)
{
    static xeno_log_stream s = {0};
    return &s;
}

/* Write formatted text to stream (for compatibility) */
static inline void xeno_log_stream_printf(xeno_log_stream *s, const char *fmt, ...)
{
    (void)s;
    va_list ap;
    va_start(ap, fmt);
    logging_info(fmt, ap);
    va_end(ap);
}

/* Non-inline symbol (implemented in src/xeno_log_stream.c) */
void xeno_log_stream(const char *fmt, ...);

#endif // XENO_LOG_H_
