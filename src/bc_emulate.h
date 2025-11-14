// SPDX-License-Identifier: MIT
// ExynosTools: BC decode interface

#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BC1 = 0,
    BC3,
    BC4,
    BC5,
    BC6H,
    BC7,
    XENO_BC_FORMAT_COUNT
} XenoBCFormat;

typedef struct {
    uint32_t baseMipLevel;
    uint32_t mipLevelCount;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;
} XenoSubresourceRange;

typedef struct XenoBCContext {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkPipelineCache pipelineCache;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    VkPipeline pipelines[XENO_BC_FORMAT_COUNT];
    VkImageView dstView;
    uint32_t workgroup_size;
    int initialized;

    // Optional bindless path
    VkDescriptorSetLayout bindlessLayout;
    VkDescriptorPool bindlessPool;
    VkDescriptorSet bindlessSet;
    int hasDescriptorIndexing;
    int hasRayTracing;
} XenoBCContext;

XenoBCContext* xeno_bc_create_context(VkDevice device, VkPhysicalDevice phys);
void xeno_bc_destroy_context(XenoBCContext* ctx);

VkResult xeno_bc_decode_image(VkCommandBuffer cmd, XenoBCContext* ctx,
                              VkBuffer src_bc, VkImage dst_rgba,
                              XenoBCFormat format, VkExtent3D extent,
                              XenoSubresourceRange subres);

#ifdef __cplusplus
}
#endif
