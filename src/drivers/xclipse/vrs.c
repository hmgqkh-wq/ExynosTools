// src/drivers/xclipse/vrs.c
// Full drop-in replacement for VRS handling (Xclipse driver).
// - Uses only declared Vulkan typedefs present in vulkan_core.h
// - Calls vkCmdSetFragmentShadingRateKHR (KHR) when available, converting NV enum to VkExtent2D
// - Calls vkCmdSetFragmentShadingRateEnumNV (NV) when available
// - No undeclared identifiers, C99-compliant, defensive checks for proc availability

#include <vulkan/vulkan.h>
#include "xeno_log.h"

/* Conservative mapping of VkFragmentShadingRateNV enum values to VkExtent2D.
   Choose safe sizes that represent the intended per-pixel reductions. */
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

/* Public API: set VRS for a command buffer given an NV enum value.
   Preference order:
     1) KHR entry (vkCmdSetFragmentShadingRateKHR) using VkExtent2D
     2) NV enum entry (vkCmdSetFragmentShadingRateEnumNV) using enum
   If neither is available, the call is a no-op and logs info.
*/
void xclipse_vrs_set_rate(VkCommandBuffer cmd, VkFragmentShadingRateNV rate)
{
    if (cmd == VK_NULL_HANDLE) return;

    /* Attempt to resolve the KHR function (expects VkExtent2D*).
       Use vkGetDeviceProcAddr with a NULL device to try the loader; if your build
       environment requires a real device handle, callers should resolve and cache
       these function pointers at device creation time instead. */
    PFN_vkCmdSetFragmentShadingRateKHR fnKHR =
        (PFN_vkCmdSetFragmentShadingRateKHR)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetFragmentShadingRateKHR");
    if (fnKHR) {
        VkExtent2D ext = xclipse_vrs_nv_to_extent(rate);
        fnKHR(cmd, &ext, NULL);
        return;
    }

    /* If KHR variant is not present, try NV enum variant that accepts the enum directly.
       The NV symbol name in headers is vkCmdSetFragmentShadingRateEnumNV (not the NV typedef name).
       Resolve and call it if available. */
    PFN_vkCmdSetFragmentShadingRateEnumNV fnEnumNV =
        (PFN_vkCmdSetFragmentShadingRateEnumNV)vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetFragmentShadingRateEnumNV");
    if (fnEnumNV) {
        fnEnumNV(cmd, rate, NULL);
        return;
    }

    XENO_LOGI("xclipse_vrs_set_rate: no VRS entrypoint available on device");
}
