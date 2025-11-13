#ifndef BC_EMULATE_H
#define BC_EMULATE_H

#include <vulkan/vulkan.h>

typedef enum {
    XENO_BC1 = 0,
    XENO_BC3 = 1,
    XENO_BC4 = 2,
    XENO_BC5 = 3,
    XENO_BC6H = 4,
    XENO_BC7 = 5,
    XENO_BC_FORMAT_COUNT = 6
} XenoBCFormat;

typedef struct {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPipelineCache pipelineCache;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkPipeline pipelines[XENO_BC_FORMAT_COUNT];
    uint32_t workgroup_size;
    int initialized;
} XenoBCContext;

typedef struct {
    uint32_t baseMipLevel;
    uint32_t mipLevelCount;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;
} XenoSubresourceRange;

XenoBCContext* xeno_bc_create_context(VkDevice device, VkPhysicalDevice phys);
void xeno_bc_destroy_context(XenoBCContext* ctx);
VkResult xeno_bc_decode_image(VkCommandBuffer cmd, XenoBCContext* ctx, VkBuffer src_bc, VkImage dst_rgba,
                              XenoBCFormat format, VkExtent3D extent, XenoSubresourceRange subres);

#endif
