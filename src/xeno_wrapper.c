// src/xeno_wrapper.c
// Full drop-in replacement for xeno_wrapper.c
// Matches the public prototype in include/xeno_bc.h exactly.

#include <vulkan/vulkan.h>
#include <stdio.h>
#include "xeno_log.h"
#include "xeno_bc.h"

/* External original functions provided by the loader or platform.
   These are expected to be resolved at link/runtime by your environment. */
extern PFN_vkCreateDevice vkCreateDevice_original;
extern PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass_original;

/* Use the exact prototype from include/xeno_bc.h */
extern VkResult xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical, VkQueue queue, struct XenoBCContext **out_ctx);
extern void xeno_bc_destroy_context(struct XenoBCContext *ctx);

/* Wrapper that creates a device and attempts to initialize the BC context.
   This strictly follows the header's xeno_bc_create_context signature. */
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

    /* Best-effort: create BC context if a queue is available from the created device.
       We query a queue index 0 for the first queue family present in pCreateInfo (if available). */
    struct XenoBCContext *bc_ctx = NULL;
    VkQueue queue = VK_NULL_HANDLE;

    if (pCreateInfo && pCreateInfo->queueCreateInfoCount > 0 && pCreateInfo->pQueueCreateInfos) {
        /* Request the first queue from the created device (family 0, index 0) */
        vkGetDeviceQueue(*pDevice, 0u, 0u, &queue);
    }

    /* Call the header-matching function. If it fails, continue — non-fatal for wrapper. */
    res = xeno_bc_create_context(*pDevice, physicalDevice, queue, &bc_ctx);
    if (res != VK_SUCCESS) {
        XENO_LOGI("xeno_wrapper_create_device: xeno_bc_create_context not available or failed (code %d) — continuing without BC context", res);
    } else {
        XENO_LOGI("xeno_wrapper_create_device: xeno_bc context created");
        /* If you need to stash bc_ctx globally, do so here; leaving local to avoid assumptions. */
        /* Example: global_bc_ctx = bc_ctx; */
        (void)bc_ctx;
    }

    return VK_SUCCESS;
}

/* Wrapper for beginning a render pass that forwards to the original begin call.
   This keeps behavior simple and avoids implicit declarations. */
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

/* Optional destroy helper that will clean up BC context if needed. */
void xeno_wrapper_destroy(struct XenoBCContext *maybe_ctx)
{
    if (maybe_ctx) {
        xeno_bc_destroy_context(maybe_ctx);
        XENO_LOGI("xeno_wrapper_destroy: BC context destroyed");
    } else {
        XENO_LOGI("xeno_wrapper_destroy: nothing to destroy");
    }
}
