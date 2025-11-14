// src/drivers/xclipse/async.c
#include "xeno_log.h"
#include "logging.h"
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stddef.h>

VkResult create_async_queue(VkDevice device, VkQueue* queue, VkCommandPool* pool)
{
    if (!device || !queue || !pool) return VK_ERROR_INITIALIZATION_FAILED;

    uint32_t queueFamilyIndex = 0;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, queue);

    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };
    return vkCreateCommandPool(device, &poolInfo, NULL, pool);
}

VkResult async_decode_submit(VkQueue queue, VkCommandBuffer cmd)
{
    if (queue == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL
    };

    VkResult res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        XENO_LOGE("Async submit failed: %d", res);
    }
    return res;
}
