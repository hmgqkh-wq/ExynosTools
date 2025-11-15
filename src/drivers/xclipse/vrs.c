// src/drivers/xclipse/vrs.c
// Full drop-in replacement for VRS handling for Xclipse driver.
// - Uses VkFragmentShadingRateNV where appropriate
// - Converts to VkExtent2D when calling vkCmdSetFragmentShadingRateKHR
// - C99-compatible, defensive checks for proc availability

#include <vulkan/vulkan.h>
#include "xeno_log.h"

void xclipse_vrs_set_rate_nv(VkCommandBuffer cmd, VkFragmentShadingRateNV rate)
{
    // If the KHR function is available, convert rate to VkExtent2D and call it.
    // Note: vkCmdSetFragmentShadingRateKHR expects const VkExtent2D* for fragment size.
    PFN_vkCmdSetFragmentShadingRateKHR fnKHR = (PFN_vkCmdSetFragmentShadingRateKHR)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetFragmentShadingRateKHR");
    if (fnKHR) {
        VkExtent2D ext = { .width = (uint32_t)rate, .height = 1u }; // conservative mapping
        // When using NV API directly, the NV call may be different; prefer KHR if available.
        fnKHR(cmd, &ext, NULL);
        return;
    }

    // Try NV entry if present (best-effort). Map to the NV-specific entry if loaded via device proc.
    PFN_vkCmdSetFragmentShadingRateNV fnNV = (PFN_vkCmdSetFragmentShadingRateNV)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetFragmentShadingRateNV");
    if (fnNV) {
        fnNV(cmd, rate, NULL);
        return;
    }

    XENO_LOGI("xclipse_vrs_set_rate_nv: no VRS entrypoint available");
}
