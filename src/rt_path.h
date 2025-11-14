#ifndef RT_PATH_H
#define RT_PATH_H

#include <vulkan/vulkan.h>

typedef struct {
    VkPipeline rtPipeline;
    VkPipelineLayout rtLayout;

    VkBuffer sbtBuffer;
    VkDeviceMemory sbtMemory;
    VkStridedDeviceAddressRegionKHR rgenRegion;
    VkStridedDeviceAddressRegionKHR missRegion;
    VkStridedDeviceAddressRegionKHR hitRegion;
    VkStridedDeviceAddressRegionKHR callRegion;

    VkAccelerationStructureKHR tlas;
    VkAccelerationStructureKHR blas;
    VkBuffer asScratch;
    VkDeviceMemory asScratchMem;

    int ready;
} XenoRT;

VkResult xeno_rt_init(VkDevice device, VkPhysicalDevice phys, XenoRT* out);
void     xeno_rt_destroy(VkDevice device, XenoRT* rt);
VkResult xeno_rt_dispatch(VkCommandBuffer cmd, XenoRT* rt, uint32_t width, uint32_t height);

#endif
