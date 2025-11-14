#include "xeno_wrapper.h"
#include "bc_emulate.h"
#include <vulkan/vulkan.h>

VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    VkResult res = vkCreateDevice_original(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res == VK_SUCCESS) {
        // Initialize context
        XenoBCContext* ctx = xeno_bc_create_context(*pDevice, physicalDevice);
    }
    return res;
}

void vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBeginInfo, VkSubpassContents contents) {
    apply_vrs(commandBuffer, pRenderPassBeginInfo->renderArea.extent);  // Apply VRS
    vkCmdBeginRenderPass_original(commandBuffer, pRenderPassBeginInfo, contents);
}

// Add async submit
void async_decode_submit(VkQueue queue, VkCommandBuffer cmd) {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
}
