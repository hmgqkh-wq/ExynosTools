// src/drivers/xclipse/vrs.c
#include <vulkan/vulkan.h>
#include "xeno_log.h"

/* Map VkFragmentShadingRateNV enum to conservative VkExtent2D */
static VkExtent2D xclipse_vrs_nv_to_extent(VkFragmentShadingRateNV rate)
{
    switch (rate) {
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV:         return (VkExtent2D){1u, 1u};
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_1X2_PIXELS_NV:    return (VkExtent2D){1u, 2u};
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_2X1_PIXELS_NV:    return (VkExtent2D){2u, 1u};
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV:    return (VkExtent2D){2u, 2u};
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_4X2_PIXELS_NV:    return (VkExtent2D){4u, 2u};
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_2X4_PIXELS_NV:    return (VkExtent2D){2u, 4u};
    case VK_FRAGMENT_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV:    return (VkExtent2D){4u, 4u};
    default:                                                         return (VkExtent2D){1u, 1u};
    }
}

void xclipse_vrs_set_rate(VkCommandBuffer cmd, VkFragmentShadingRateNV rate)
{
    if (cmd == VK_NULL_HANDLE) return;

    PFN_vkCmdSetFragmentShadingRateKHR fnKHR =
        (PFN_vkCmdSetFragmentShadingRateKHR)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetFragmentShadingRateKHR");
    if (fnKHR) {
        VkExtent2D ext = xclipse_vrs_nv_to_extent(rate);
        fnKHR(cmd, &ext, NULL);
        return;
    }

    PFN_vkCmdSetFragmentShadingRateEnumNV fnEnumNV =
        (PFN_vkCmdSetFragmentShadingRateEnumNV)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetFragmentShadingRateEnumNV");
    if (fnEnumNV) {
        fnEnumNV(cmd, rate, NULL);
        return;
    }

    XENO_LOGI("xclipse_vrs_set_rate: no VRS entrypoint available on device");
}
