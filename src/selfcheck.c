#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "xeno_log.h"
#include "logging.h"

#define DECL_WEAK_SHADER(name) \
  __attribute__((weak)) extern const uint32_t name##_spv[]; \
  __attribute__((weak)) extern const size_t   name##_spv_len;

DECL_WEAK_SHADER(bc1)
DECL_WEAK_SHADER(bc2)
DECL_WEAK_SHADER(bc3)
DECL_WEAK_SHADER(bc4)
DECL_WEAK_SHADER(bc5)
DECL_WEAK_SHADER(bc6h)
DECL_WEAK_SHADER(bc7)

static void check_shader(const char *label, const uint32_t *arr, const size_t *len)
{
    if (arr && len && *len > 0) {
        printf("Shader %-4s: PRESENT  words=%zu  approx_bytes=%zu\n",
               label, *len, *len * sizeof(uint32_t));
    } else {
        printf("Shader %-4s: MISSING\n", label);
    }
}

int main(void)
{
    printf("=== ExynosTools self-check ===\n");
    FILE *stream = xeno_log_stream();
    int dbg = xeno_log_enabled_debug();
    printf("Logging: stream=%s  debug_enabled=%d\n", stream ? "OK" : "NULL", dbg);
    XENO_LOGI("Self-check starting");

    printf("Shaders (generated_includes):\n");
    check_shader("BC1", bc1_spv, &bc1_spv_len);
    check_shader("BC2", bc2_spv, &bc2_spv_len);
    check_shader("BC3", bc3_spv, &bc3_spv_len);
    check_shader("BC4", bc4_spv, &bc4_spv_len);
    check_shader("BC5", bc5_spv, &bc5_spv_len);
    check_shader("BC6H", bc6h_spv, &bc6h_spv_len);
    check_shader("BC7", bc7_spv, &bc7_spv_len);

    int present_count = 0;
    present_count += (bc1_spv && bc1_spv_len > 0);
    present_count += (bc2_spv && bc2_spv_len > 0);
    present_count += (bc3_spv && bc3_spv_len > 0);
    present_count += (bc4_spv && bc4_spv_len > 0);
    present_count += (bc5_spv && bc5_spv_len > 0);
    present_count += (bc6h_spv && bc6h_spv_len > 0);
    present_count += (bc7_spv && bc7_spv_len > 0);

    if (present_count == 7) {
        printf("Result: All BC shaders PRESENT. Binary likely feature-complete for decode pipelines.\n");
    } else {
        printf("Result: Only %d/7 shaders present. Binary is minimal or missing features.\n", present_count);
    }

    printf("=== Self-check done ===\n");
    return 0;
}
