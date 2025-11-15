/*
  src/app_profile.c
  Application profile detection stub.
*/
#include "logging.h"

int app_profile_detect(const char *app_name) {
    if (!app_name) return 0;
    logging_info("App profile detection: %s", app_name);
    return 1; // always returns detected for now
}
