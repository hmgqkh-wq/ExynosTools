// include/xeno_log.h
#ifndef XENO_LOG_H_
#define XENO_LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include "logging.h"

#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#define XENO_LOGI(...) logging_info(__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

FILE *xeno_log_stream(void);

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
