// src/xeno_log_stream.c
// Provide the legacy symbol FILE* xeno_log_stream(void).
// This returns stderr so old code that writes to that stream still works.
// For formatted helper calls, include xeno_log.h which maps printing to logging_info.

#include <stdio.h>
#include "xeno_log.h"

/* Return a FILE* compatible stream used by legacy code.
   stderr is chosen so messages are visible in CI logs. */
FILE *xeno_log_stream(void)
{
    return stderr;
}
