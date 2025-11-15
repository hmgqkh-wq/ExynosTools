// src/xeno_wrapper.c
// Full drop-in implementation matching include/xeno_wrapper.h prototypes.

#include <vulkan/vulkan.h>
#include "xeno_wrapper.h"
#include "xeno_log.h"
#include "xeno_bc.h"

/* If real loader-originals exist, declare them here. If not, leave them NULL.
   Replace or initialize these externs in your loader/initialization code if needed. */
extern PFN_vkCreateDevice vkCreateDevice_original;
extern PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass_original;

/* Create device wrapper: calls original, then attempts to create BC context. */
VkResult xeno_wrapper_create_device(VkPhysicalDevice physicalDevice,
                                    const VkDeviceCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator,
                                    VkDevice *pDevice)
{
    if (!vkCreateDevice_original) {
        XENO_LOGE("xeno_wrapper_create_device: vkCreateDevice_original not available");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = vkCreateDevice_original(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res != VK_SUCCESS) {
        XENO_LOGE("xeno_wrapper_create_device: vkCreateDevice_original failed: %d", res);
        return res;
    }

    struct XenoBCContext *bc_ctx = NULL;
    VkQueue queue = VK_NULL_HANDLE;

    if (pCreateInfo && pCreateInfo->queueCreateInfoCount > 0 && pCreateInfo->pQueueCreateInfos) {
        vkGetDeviceQueue(*pDevice, 0u, 0u, &queue);
    }

    res = xeno_bc_create_context(*pDevice, physicalDevice, queue, &bc_ctx);
    if (res != VK_SUCCESS) {
        XENO_LOGI("xeno_wrapper_create_device: xeno_bc_create_context not available or failed (code %d) — continuing without BC context", res);
    } else {
        XENO_LOGI("xeno_wrapper_create_device: xeno_bc context created");
        (void)bc_ctx;
    }

    return VK_SUCCESS;
}

/* Begin render wrapper forwards to original begin render if present. */
void xeno_wrapper_begin_render(VkCommandBuffer commandBuffer,
                               const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                               VkSubpassContents contents)
{
    if (vkCmdBeginRenderPass_original) {
        vkCmdBeginRenderPass_original(commandBuffer, pRenderPassBeginInfo, contents);
    } else {
        XENO_LOGW("xeno_wrapper_begin_render: original vkCmdBeginRenderPass not available");
    }
}

/* Destroy wrapper: destroy BC context if provided. */
void xeno_wrapper_destroy(struct XenoBCContext *maybe_ctx)
{
    if (maybe_ctx) {
        xeno_bc_destroy_context(maybe_ctx);
        XENO_LOGI("xeno_wrapper_destroy: BC context destroyed");
    } else {
        XENO_LOGI("xeno_wrapper_destroy: nothing to destroy");
    }
}
