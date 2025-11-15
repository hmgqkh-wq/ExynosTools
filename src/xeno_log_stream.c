#include <stdio.h>

static FILE* g_log_stream = NULL;

FILE* xeno_log_stream(void) {
    if (!g_log_stream) g_log_stream = stderr;
    return g_log_stream;
}

int xeno_log_enabled_debug(void) {
    return 1; /* enable debug by default */
}
