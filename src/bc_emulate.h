#ifndef BC_EMULATE_H
#define BC_EMULATE_H

#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

// Supported BC formats (keep in sync with shader/order in bc_emulate.c)
typedef enum XenoBCFormat {
    XENO_BC1 = 0,
    XENO_BC3 = 1,
    XENO_BC4 = 2,
    XENO_BC5 = 3,
    XENO_BC6H = 4,
    XENO_BC7 = 5,
    XENO_BC_FORMAT_COUNT = 6
} XenoBCFormat;

// Subresource selection
typedef struct XenoSubresourceRange {
    uint32_t baseMipLevel;
    uint32_t mipLevelCount;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;
} XenoSubresourceRange;

// Driver context for BC emulation
typedef struct XenoBCContext {
    VkDevice device;
    VkPhysicalDevice physicalDevice;

    VkPipeline pipelines[XENO_BC_FORMAT_COUNT];
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkPipelineCache pipelineCache;

    uint32_t workgroup_size;

    // Storage image view for dst_rgba (created on demand, reused, destroyed at teardown)
    VkImageView dstView;

    int initialized;
} XenoBCContext;

// Lifecycle
XenoBCContext* xeno_bc_create_context(VkDevice device, VkPhysicalDevice phys);
void           xeno_bc_destroy_context(XenoBCContext* ctx);

// Decode entry
VkResult xeno_bc_decode_image(VkCommandBuffer cmd,
                              XenoBCContext* ctx,
                              VkBuffer src_bc,
                              VkImage dst_rgba,
                              XenoBCFormat format,
                              VkExtent3D extent,
                              XenoSubresourceRange subres);

#ifdef __cplusplus
}
#endif

#endif // BC_EMULATE_H
