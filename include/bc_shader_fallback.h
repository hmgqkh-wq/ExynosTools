// include/bc_shaders_fallback.h
// Fallback extern declarations for BC shader SPIR-V arrays and lengths.
// Safe no-op declarations to avoid "undeclared identifier" compile errors
// when generated headers are not present in the include path.

#ifndef BC_SHADERS_FALLBACK_H_
#define BC_SHADERS_FALLBACK_H_

#include <stdint.h>
#include <stddef.h>

/* If the real generated headers are present they will define these symbols.
   These are extern fallbacks that do not create storage. */
extern const uint32_t bc1_shader_spv[];  extern const size_t bc1_shader_spv_len;
extern const uint32_t bc2_shader_spv[];  extern const size_t bc2_shader_spv_len;
extern const uint32_t bc3_shader_spv[];  extern const size_t bc3_shader_spv_len;
extern const uint32_t bc4_shader_spv[];  extern const size_t bc4_shader_spv_len;
extern const uint32_t bc5_shader_spv[];  extern const size_t bc5_shader_spv_len;
extern const uint32_t bc6h_shader_spv[]; extern const size_t bc6h_shader_spv_len;
extern const uint32_t bc7_shader_spv[];  extern const size_t bc7_shader_spv_len;

#endif // BC_SHADERS_FALLBACK_H_
