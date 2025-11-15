/*
  src/drivers/xclipse/async.c
  Async helpers for Xclipse 940.
*/
#include "logging.h"
#include <stdatomic.h>

static _Atomic int async_counter = 0;

void async_submit_begin(void) {
    atomic_fetch_add(&async_counter, 1);
    logging_info("Async submit begin (counter=%d)", async_counter);
}

void async_submit_end(void) {
    atomic_fetch_sub(&async_counter, 1);
    logging_info("Async submit end (counter=%d)", async_counter);
}
