// include/xeno_log.h
#ifndef XENO_LOG_H_
#define XENO_LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include "logging.h"

/* Prefer logging.h definitions; only define if missing */
#ifndef XENO_LOGE
#define XENO_LOGE(...) logging_error(__VA_ARGS__)
#endif

#ifndef XENO_LOGI
#define XENO_LOGI(...) logging_info(__VA_ARGS__)
#endif

#ifdef __cplusplus
extern "C" {
#endif

FILE *xeno_log_stream(void);

#ifdef __cplusplus
}
#endif

#endif // XENO_LOG_H_
