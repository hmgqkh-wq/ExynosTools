// include/xeno_log.h
// Compatibility header mapping legacy logging names to the current logging API.
// This header preserves the original ABI: xeno_log_stream() returns FILE*

#ifndef XENO_LOG_H_
#define XENO_LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include "logging.h"

/* Map legacy macro names to current logger */
#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#define XENO_LOGI(...) logging_info(__VA_ARGS__)

/* Preserve original declaration expected by older code:
   FILE* xeno_log_stream(void);  */
#ifdef __cplusplus
extern "C" {
#endif

FILE *xeno_log_stream(void);

/* Helper function that older code sometimes uses to write formatted text
   to the legacy stream. This is inline and forwards to logging_info. */
static inline void xeno_log_stream_printf(FILE *stream, const char *fmt, ...)
{
    (void)stream;
    va_list ap;
    va_start(ap, fmt);
    logging_info(fmt, ap);
    va_end(ap);
}

#ifdef __cplusplus
}
#endif

#endif // XENO_LOG_H_
