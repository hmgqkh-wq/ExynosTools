#ifndef XENO_WRAPPER_H
#define XENO_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <vulkan/vulkan.h>

#define XENO_WRAPPER_VERSION_MAJOR 1
#define XENO_WRAPPER_VERSION_MINOR 1
#define XENO_WRAPPER_VERSION_PATCH 0

typedef enum XenoWrapperCaps {
    XENO_CAP_PIPELINE_CACHE_PERSIST   = 1u << 0,
    XENO_CAP_DESCRIPTOR_REUSE         = 1u << 1,
    XENO_CAP_FEATURE_NORMALIZATION    = 1u << 2,
    XENO_CAP_BC_DECODE_COMPUTE        = 1u << 3,
    XENO_CAP_SPECIALIZATION_CONSTANTS = 1u << 4,
    XENO_CAP_ASYNC_PIPELINE_CREATION  = 1u << 5,
    XENO_CAP_SPIRV_VALIDATION         = 1u << 6
} XenoWrapperCaps;

uint32_t xeno_wrapper_get_caps(void);
VkResult xeno_wrapper_validate_spirv(const uint32_t* words, uint32_t byte_len);
VkResult xeno_wrapper_warmup(VkDevice device);
VkResult xeno_wrapper_save_pipeline_cache(VkDevice device, VkPipelineCache cache, const char* path);
VkResult xeno_wrapper_load_pipeline_cache(VkDevice device, const char* path, VkPipelineCache* out_cache);

static inline uint32_t xeno_wrapper_version_u32(void) {
    return (XENO_WRAPPER_VERSION_MAJOR << 20) |
           (XENO_WRAPPER_VERSION_MINOR << 10) |
           (XENO_WRAPPER_VERSION_PATCH);
}

#ifdef __cplusplus
}
#endif

#endif
