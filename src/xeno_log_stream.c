// src/xeno_log_stream.c
// Exported function to satisfy legacy references to xeno_log_stream.
// Forwards formatted messages into the project's logging_info implementation.

#include <stdarg.h>
#include "logging.h"

/* Exported function called like: xeno_log_stream("fmt %d", ...);
   For compatibility with older code we forward to logging_info. */
void xeno_log_stream(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    logging_info(fmt, ap);
    va_end(ap);
}
