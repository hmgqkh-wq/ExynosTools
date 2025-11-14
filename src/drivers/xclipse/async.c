// src/drivers/xclipse/async.c
// Lightweight async helpers for Xclipse driver path.
// Provides a simple create_async_queue and async_decode_submit helper.
// Includes compatibility logging header so XENO_LOGE / XENO_LOGI and xeno_log_stream are available.

#include "xeno_log.h"      // maps XENO_LOGE/XENO_LOGI and declares xeno_log_stream()
#include "logging.h"
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stddef.h>

VkResult create_async_queue(VkDevice device, VkQueue* queue, VkCommandPool* pool)
{
    if (!device || !queue || !pool) return VK_ERROR_INITIALIZATION_FAILED;

    /* NOTE: caller should set the correct queue family index when available.
       For simple CI/pathing we default to 0 (common for compute on many setups). */
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
