// src/xeno_log_stream.c
// Minimal non-inline implementation to satisfy undefined references to xeno_log_stream
// defined by legacy code. This forwards to logging_error/info as appropriate.

#include <stdarg.h>
#include "logging.h"

/* Fallback global function used by older files that call xeno_log_stream(fmt, ...).
   We forward to logging_error for now so important messages are visible in CI logs. */
void xeno_log_stream(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    logging_error(fmt, ap);
    va_end(ap);
}
