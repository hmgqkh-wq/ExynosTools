// SPDX-License-Identifier: MIT
// include/bc5_shader.h
// Ready-to-copy minimal placeholder header for BC5 shader SPIR-V data.

#ifndef BC5_SHADER_H
#define BC5_SHADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t bc5_spv[];
extern const size_t bc5_spv_size;

#ifdef BC5_SHADER_HEADER_DEFINE_DEFAULTS

const uint32_t bc5_spv[] = {
    0x07230203u, 0x00010000u, 0x0008000bu
};
const size_t bc5_spv_size = sizeof(bc5_spv);

#endif /* BC5_SHADER_HEADER_DEFINE_DEFAULTS */

#ifdef __cplusplus
}
#endif

#endif /* BC5_SHADER_H */
