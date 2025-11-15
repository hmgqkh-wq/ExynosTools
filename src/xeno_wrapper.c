#include <vulkan/vulkan.h>
#include "xeno_bc.h"
#include "xeno_log.h"

// Declare external functions used but not defined here
extern VkResult vkCreateDevice_original(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
extern void apply_vrs(VkCommandBuffer, VkExtent2D);
extern void vkCmdBeginRenderPass_original(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
extern void XENO_LOGE(const char *fmt, ...);

VkResult xeno_wrapper_create_device(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    VkResult res = vkCreateDevice_original(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res != VK_SUCCESS) {
        XENO_LOGE("Async submit failed: %d", res);
        return res;
    }

    // Optional: initialize BC context
    // XenoBCContext* ctx = xeno_bc_create_context(*pDevice, physicalDevice); // removed unused

    return VK_SUCCESS;
}

void xeno_wrapper_begin_render(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                               VkSubpassContents contents) {
    apply_vrs(commandBuffer, pRenderPassBeginInfo->renderArea.extent); // Apply VRS
    vkCmdBeginRenderPass_original(commandBuffer, pRenderPassBeginInfo, contents);
}
