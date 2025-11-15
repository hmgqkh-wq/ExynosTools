// src/xeno_wrapper.c
// Full drop-in replacement for xeno_wrapper.c
// - Wraps vkCreateDevice and a render-pass begin call used by the project
// - Declares required externs and uses minimal logic to initialize a XenoBCContext
// - C99-compliant, avoids implicit declarations and unused warnings

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <stdio.h>
#include "xeno_bc.h"
#include "xeno_log.h"

/* External originals and utilities the wrapper forwards to.
   These symbols are expected to be provided by the platform or loader. */
extern PFN_vkCreateDevice vkCreateDevice_original;
extern PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass_original;
extern PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr_original;

/* Forward declaration of optionally provided functions in other modules.
   If they are not present at link-time, provide weak stubs below. */
extern XenoBCContext* xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical);
extern void apply_vrs(VkCommandBuffer cmd, VkExtent2D extent);

/* Weak stubs to avoid link errors if the real implementations are omitted.
   Use attribute weak where available; otherwise provide non-fatal defaults. */
#if defined(__GNUC__)
__attribute__((weak))
#endif
XenoBCContext* xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical) {
    (void)device; (void)physical;
    return NULL;
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void apply_vrs(VkCommandBuffer cmd, VkExtent2D extent) {
    (void)cmd; (void)extent; /* no-op fallback */
}

/* Safe wrapper for vkCreateDevice.
   Calls the original implementation, then attempts to create Xeno BC context
   (best-effort; non-fatal if context creation fails). */
VkResult xeno_wrapper_create_device(VkPhysicalDevice physicalDevice,
                                    const VkDeviceCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator,
                                    VkDevice *pDevice)
{
    if (!vkCreateDevice_original) {
        XENO_LOGE("xeno_wrapper: vkCreateDevice_original not available");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = vkCreateDevice_original(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res != VK_SUCCESS) {
        XENO_LOGE("xeno_wrapper: vkCreateDevice_original failed: %d", res);
        return res;
    }

    /* Best-effort: create BC context if implementation is present. */
    XenoBCContext* ctx = xeno_bc_create_context(*pDevice, physicalDevice);
    if (!ctx) {
        XENO_LOGI("xeno_wrapper: xeno_bc_create_context not available or failed (non-fatal)");
    } else {
        XENO_LOGI("xeno_wrapper: xeno_bc context created");
        /* Optionally store context somewhere global if needed by other wrappers */
    }

    return VK_SUCCESS;
}

/* Wrapper for beginning a render pass that applies VRS prior to forwarding. */
void xeno_wrapper_begin_render(VkCommandBuffer commandBuffer,
                               const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                               VkSubpassContents contents)
{
    if (!pRenderPassBeginInfo) {
        XENO_LOGE("xeno_wrapper_begin_render: null render pass info");
        return;
    }

    /* Apply variable-rate shading or other instrumentation if implemented */
    apply_vrs(commandBuffer, pRenderPassBeginInfo->renderArea.extent);

    /* Forward to original begin render pass if available */
    if (vkCmdBeginRenderPass_original) {
        vkCmdBeginRenderPass_original(commandBuffer, pRenderPassBeginInfo, contents);
    } else {
        XENO_LOGW("xeno_wrapper_begin_render: original vkCmdBeginRenderPass not available");
    }
}

/* Exported no-op cleanup helper (keeps symbol present and avoids warnings). */
void xeno_wrapper_destroy(void)
{
    /* If xeno_bc had global cleanup, call it here (not present in stub). */
    XENO_LOGI("xeno_wrapper: destroy called");
}
