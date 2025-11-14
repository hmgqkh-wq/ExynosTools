// SPDX-License-Identifier: MIT
// include/bc4_shader.h
// Ready-to-copy minimal placeholder header for BC4 shader SPIR-V data.

#ifndef BC4_SHADER_H
#define BC4_SHADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t bc4_spv[];
extern const size_t bc4_spv_size;

#ifdef BC4_SHADER_HEADER_DEFINE_DEFAULTS

const uint32_t bc4_spv[] = {
    0x07230203u, 0x00010000u, 0x0008000bu
};
const size_t bc4_spv_size = sizeof(bc4_spv);

#endif /* BC4_SHADER_HEADER_DEFINE_DEFAULTS */

#ifdef __cplusplus
}
#endif

#endif /* BC4_SHADER_H */
