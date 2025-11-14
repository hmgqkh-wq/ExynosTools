#include <vulkan/vulkan.h>
#include "logging.h"

VkResult create_async_queue(VkDevice device, VkQueue* queue, VkCommandPool* pool) {
    uint32_t queueFamilyIndex = 0;  // Assume compute queue 0 for Xclipse
    vkGetDeviceQueue(device, queueFamilyIndex, 0, queue);

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };
    return vkCreateCommandPool(device, &poolInfo, NULL, pool);
}

VkResult async_decode_submit(VkQueue queue, VkCommandBuffer cmd) {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    VkResult res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        XENO_LOGE("Async submit failed: %d", res);
    }
    return res;
}
