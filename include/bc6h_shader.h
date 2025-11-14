// SPDX-License-Identifier: MIT
// include/bc6h_shader.h
// Ready-to-copy minimal placeholder header for BC6H shader SPIR-V data.

#ifndef BC6H_SHADER_H
#define BC6H_SHADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t bc6h_spv[];
extern const size_t bc6h_spv_size;

#ifdef BC6H_SHADER_HEADER_DEFINE_DEFAULTS

const uint32_t bc6h_spv[] = {
    0x07230203u, 0x00010000u, 0x0008000bu
};
const size_t bc6h_spv_size = sizeof(bc6h_spv);

#endif /* BC6H_SHADER_HEADER_DEFINE_DEFAULTS */

#ifdef __cplusplus
}
#endif

#endif /* BC6H_SHADER_H */
