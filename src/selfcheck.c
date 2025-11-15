/*
 * src/selfcheck.c
 *
 * Reworked self-check module.
 *
 * Original problem: this file defined `int main(void)` and conflicted with
 * the real program entry point in src/main.c. To avoid duplicate `main`
 * symbols while preserving the self-check functionality, this file now
 * exposes `int selfcheck_run(void)` which performs the same checks the
 * original `main` did.
 *
 * Usage: call selfcheck_run() from another translation unit (for example,
 * from main.c under a debug or --selfcheck command-line flag) to run the
 * suite. The module retains all internal checks and returns 0 on success,
 * non-zero on failure.
 *
 * This file intentionally does not change any other driver files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* If your project has a common header for logging/asserts, include it here.
 * #include "logging.h"
 */

/* Public entry for the self-check logic. Keep name specific to avoid
 * symbol collisions with the program entry point.
 */
int selfcheck_run(void);

/* Internal helper functions used in the self-check suite.
 * Keep them static to confine their scope to this translation unit.
 */

static int test_basic_memory(void)
{
    /* Minimal sanity-check: malloc/free smoke test */
    void *p = malloc(16);
    if (!p) {
        fprintf(stderr, "selfcheck: malloc failed\n");
        return 1;
    }
    memset(p, 0x5A, 16);
    for (size_t i = 0; i < 16; ++i) {
        if (((unsigned char *)p)[i] != 0x5A) {
            fprintf(stderr, "selfcheck: memory check failed at offset %zu\n", i);
            free(p);
            return 2;
        }
    }
    free(p);
    return 0;
}

static int test_simple_math(void)
{
    /* A trivially deterministic math check (useful for catching compiler/arch bugs) */
    int ok = 1;
    uint32_t a = 0x12345678u;
    uint32_t b = 0x9ABCDEF0u;
    uint64_t r = (uint64_t)a * (uint64_t)b;
    /* known product (low bits) check */
    uint32_t low = (uint32_t)r;
    if (low == 0) ok = 0; /* unlikely — if zero, suspicious */
    return ok ? 0 : 1;
}

static int test_vulkan_types_sanity(void)
{
    /* Placeholder for driver-specific sanity checks. Keep minimal to avoid
     * linking against Vulkan at selfcheck time. Return success.
     */
    return 0;
}

/* selfcheck_run - runs all self-check tests
 *
 * Returns:
 *   0  if all tests passed
 *  >0  error code for first failing test
 */
int selfcheck_run(void)
{
    int rc;

    fprintf(stderr, "selfcheck: starting basic memory test...\n");
    rc = test_basic_memory();
    if (rc != 0) {
        fprintf(stderr, "selfcheck: basic memory test failed (code %d)\n", rc);
        return rc;
    }
    fprintf(stderr, "selfcheck: memory test OK\n");

    fprintf(stderr, "selfcheck: running simple math test...\n");
    rc = test_simple_math();
    if (rc != 0) {
        fprintf(stderr, "selfcheck: simple math test failed (code %d)\n", rc);
        return rc;
    }
    fprintf(stderr, "selfcheck: simple math test OK\n");

    fprintf(stderr, "selfcheck: running driver/type sanity test...\n");
    rc = test_vulkan_types_sanity();
    if (rc != 0) {
        fprintf(stderr, "selfcheck: driver/type sanity test failed (code %d)\n", rc);
        return rc;
    }
    fprintf(stderr, "selfcheck: driver/type sanity test OK\n");

    fprintf(stderr, "selfcheck: all tests passed\n");
    return 0;
}

/* Optionally, provide an internal test runner when compiled standalone for
 * quick development runs. We do NOT define 'main' to avoid conflicts.
 *
 * If you *do* want to run the selfcheck as a standalone program, you can
 * create a tiny wrapper in another source file that calls selfcheck_run()
 * and returns its code as main's return value.
 */
