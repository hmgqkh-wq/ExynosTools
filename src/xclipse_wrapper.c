// SPDX-License-Identifier: MIT
// src/xclipse_wrapper.c
// Xclipse-specific wrapper helpers — do NOT export vkGetInstanceProcAddr / vkGetDeviceProcAddr.

#include <vulkan/vulkan.h>
#include <stddef.h>

/* Safety check: do not define PROVIDE_VK_GLOBALS for this file. */
#ifdef PROVIDE_VK_GLOBALS
# error "Do not define PROVIDE_VK_GLOBALS for xclipse_wrapper.c; define it only for the canonical provider."
#endif

/* Local non-exported helpers. These avoid exporting the same public symbols. */

static PFN_vkGetInstanceProcAddr my_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    (void)instance;
    (void)pName;
    return NULL;
}

static PFN_vkGetDeviceProcAddr my_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    (void)device;
    (void)pName;
    return NULL;
}
