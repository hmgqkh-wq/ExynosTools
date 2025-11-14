// include/logging.h
// Minimal logging API used by bc_emulate.c and other sources.

#ifndef LOGGING_H_
#define LOGGING_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void logging_error(const char *fmt, ...);
void logging_info(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // LOGGING_H_
