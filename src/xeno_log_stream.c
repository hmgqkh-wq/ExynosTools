// src/xeno_log_stream.c
// Minimal exported implementation to satisfy legacy references to xeno_log_stream.
// Forwards to logging_info so messages appear in CI logs.

#include <stdarg.h>
#include "logging.h"

void xeno_log_stream(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    logging_info(fmt, ap);
    va_end(ap);
}
