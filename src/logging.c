// src/logging.c
// Minimal portable logger implementation used for CI and debugging.
// Replace with your project's real logging implementation later.

#include "logging.h"
#include <stdio.h>
#include <stdarg.h>

void logging_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void logging_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[INFO] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}
