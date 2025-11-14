// SPDX-License-Identifier: MIT
// src/minimal_wrapper.c
// Minimal wrapper helpers — do NOT export vkGetInstanceProcAddr / vkGetDeviceProcAddr.

#include <vulkan/vulkan.h>
#include <stddef.h>

/* Safety check: do not define PROVIDE_VK_GLOBALS for this file. */
#ifdef PROVIDE_VK_GLOBALS
# error "Do not define PROVIDE_VK_GLOBALS for minimal_wrapper.c; define it only for the canonical provider."
#endif

/* Local non-exported helpers. Callers in this module should call these helpers
   instead of relying on exported globals to avoid duplicate symbol issues. */

static PFN_vkGetInstanceProcAddr my_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    (void)instance;
    (void)pName;
    /* Minimal fallback: return NULL. Replace with platform-specific resolution if needed. */
    return NULL;
}

static PFN_vkGetDeviceProcAddr my_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    (void)device;
    (void)pName;
    return NULL;
}

/* If other files in this repo currently call vkGetInstanceProcAddr directly,
   change those call sites to call my_vkGetInstanceProcAddr or the canonical exported
   functions from xeno_wrapper.c as appropriate. */
