// SPDX-License-Identifier: MIT
// include/bc_common_shader.h
// Ready-to-copy minimal placeholder header for common shader utilities or shared SPIR-V blobs.

#ifndef BC_COMMON_SHADER_H
#define BC_COMMON_SHADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* If the code expects a common helper blob or symbols, declare them here.
   Keep names generic; adjust if your code references other identifiers. */

extern const uint32_t bc_common_spv[];
extern const size_t bc_common_spv_size;

#ifdef BC_COMMON_SHADER_HEADER_DEFINE_DEFAULTS

const uint32_t bc_common_spv[] = {
    0x07230203u, 0x00010000u, 0x0008000bu
};
const size_t bc_common_spv_size = sizeof(bc_common_spv);

#endif /* BC_COMMON_SHADER_HEADER_DEFINE_DEFAULTS */

#ifdef __cplusplus
}
#endif

#endif /* BC_COMMON_SHADER_H */
