// SPDX-License-Identifier: MIT
// src/minimal_wrapper.c
// Minimal wrapper helpers — do NOT export vkGetInstanceProcAddr / vkGetDeviceProcAddr.

#include <vulkan/vulkan.h>
#include <stddef.h>

/* Safety check: do not define PROVIDE_VK_GLOBALS for this file. */
#ifdef PROVIDE_VK_GLOBALS
# error "Do not define PROVIDE_VK_GLOBALS for minimal_wrapper.c; define it only for the canonical provider."
#endif

/* Local non-exported helpers. Callers in this module should use these helpers
   instead of relying on global exported symbols to avoid duplicate symbol issues. */

static PFN_vkVoidFunction my_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    (void)instance;
    (void)pName;
    return (PFN_vkVoidFunction)NULL;
}

static PFN_vkVoidFunction my_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    (void)device;
    (void)pName;
    return (PFN_vkVoidFunction)NULL;
}
