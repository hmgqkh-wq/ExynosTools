#ifndef XENO_WRAPPER_H
#define XENO_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Public ABI for the ExynosTools Vulkan wrapper.
// Keep this header minimal and stable to avoid breaking consumers.

#include <stdint.h>
#include <vulkan/vulkan.h>

// Versioning for the wrapper library (bump when public ABI changes)
#define XENO_WRAPPER_VERSION_MAJOR 1
#define XENO_WRAPPER_VERSION_MINOR 0
#define XENO_WRAPPER_VERSION_PATCH 0

// Simple capability bits that clients may query (extend as you add features)
typedef enum XenoWrapperCaps {
    XENO_CAP_PIPELINE_CACHE        = 1u << 0,
    XENO_CAP_DESCRIPTOR_REUSE      = 1u << 1,
    XENO_CAP_FEATURE_NORMALIZATION = 1u << 2,
    XENO_CAP_BC_DECODE_COMPUTE     = 1u << 3
} XenoWrapperCaps;

// Returns a bitmask of capabilities exposed by the wrapper.
// Implemented internally; provided here for apps/tools to query.
uint32_t xeno_wrapper_get_caps(void);

// Optional: expose a helper to validate a SPIR-V blob (non-fatal).
// Returns VK_SUCCESS if basic invariants look correct.
VkResult xeno_wrapper_validate_spirv(const uint32_t* words, uint32_t byte_len);

// Optional: expose a light-weight pipeline cache warm-up hook.
// It’s safe to no-op; apps can call this before heavy workloads.
VkResult xeno_wrapper_warmup(VkDevice device);

// Convenience: encoded library version as an integer
static inline uint32_t xeno_wrapper_version_u32(void) {
    return (XENO_WRAPPER_VERSION_MAJOR << 20) |
           (XENO_WRAPPER_VERSION_MINOR << 10) |
           (XENO_WRAPPER_VERSION_PATCH);
}

#ifdef __cplusplus
}
#endif

#endif // XENO_WRAPPER_H
