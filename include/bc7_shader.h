// SPDX-License-Identifier: MIT
// include/bc7_shader.h
// Ready-to-copy minimal placeholder header for BC7 shader SPIR-V data.

#ifndef BC7_SHADER_H
#define BC7_SHADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t bc7_spv[];
extern const size_t bc7_spv_size;

#ifdef BC7_SHADER_HEADER_DEFINE_DEFAULTS

const uint32_t bc7_spv[] = {
    0x07230203u, 0x00010000u, 0x0008000bu
};
const size_t bc7_spv_size = sizeof(bc7_spv);

#endif /* BC7_SHADER_HEADER_DEFINE_DEFAULTS */

#ifdef __cplusplus
}
#endif

#endif /* BC7_SHADER_H */
