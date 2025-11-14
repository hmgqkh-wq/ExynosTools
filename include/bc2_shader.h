// SPDX-License-Identifier: MIT
// include/bc2_shader.h
// Ready-to-copy minimal placeholder header for BC2 shader SPIR-V data.

#ifndef BC2_SHADER_H
#define BC2_SHADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t bc2_spv[];
extern const size_t bc2_spv_size;

#ifdef BC2_SHADER_HEADER_DEFINE_DEFAULTS

const uint32_t bc2_spv[] = {
    0x07230203u, 0x00010000u, 0x0008000bu
};
const size_t bc2_spv_size = sizeof(bc2_spv);

#endif /* BC2_SHADER_HEADER_DEFINE_DEFAULTS */

#ifdef __cplusplus
}
#endif

#endif /* BC2_SHADER_H */
