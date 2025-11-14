#ifndef LOGGING_H_
#define LOGGING_H_

#include <stdio.h>

FILE *xeno_log_stream(void);

#define logging_error(fmt, ...) fprintf(xeno_log_stream(), "[ExynosTools][E] " fmt "\n", ##__VA_ARGS__)
#define logging_info(fmt, ...)  fprintf(xeno_log_stream(), "[ExynosTools][I] " fmt "\n", ##__VA_ARGS__)

#endif // LOGGING_H_
