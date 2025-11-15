#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Log API (minimal use to avoid link issues) */
FILE* xeno_log_stream(void);
int   xeno_log_enabled_debug(void);

#define DECL_SHADER(name) \
  extern const uint32_t name##_shader_spv[]; \
  extern const size_t   name##_shader_spv_len;

DECL_SHADER(bc1)
DECL_SHADER(bc2)
DECL_SHADER(bc3)
DECL_SHADER(bc4)
DECL_SHADER(bc5)
DECL_SHADER(bc6h)
DECL_SHADER(bc7)

static void check_shader(const char *label,
                         const uint32_t *arr,
                         const size_t len_words)
{
    if (arr && len_words > 0) {
        printf("Shader %-4s: PRESENT  words=%zu  approx_bytes=%zu\n",
               label, len_words, len_words * sizeof(uint32_t));
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

    printf("Shaders (embedded arrays):\n");
    check_shader("BC1", bc1_shader_spv, bc1_shader_spv_len);
    check_shader("BC2", bc2_shader_spv, bc2_shader_spv_len);
    check_shader("BC3", bc3_shader_spv, bc3_shader_spv_len);
    check_shader("BC4", bc4_shader_spv, bc4_shader_spv_len);
    check_shader("BC5", bc5_shader_spv, bc5_shader_spv_len);
    check_shader("BC6H", bc6h_shader_spv, bc6h_shader_spv_len);
    check_shader("BC7", bc7_shader_spv, bc7_shader_spv_len);

    size_t present = 0;
    present += (bc1_shader_spv_len > 0);
    present += (bc2_shader_spv_len > 0);
    present += (bc3_shader_spv_len > 0);
    present += (bc4_shader_spv_len > 0);
    present += (bc5_shader_spv_len > 0);
    present += (bc6h_shader_spv_len > 0);
    present += (bc7_shader_spv_len > 0);

    if (present == 7) {
        printf("Result: All BC shaders PRESENT. Binary likely feature-complete for decode pipelines.\n");
    } else {
        printf("Result: Only %zu/7 shaders present. Binary is minimal or missing features.\n", present);
    }
    printf("=== Self-check done ===\n");
    return 0;
}
