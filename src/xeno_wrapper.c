// SPDX-License-Identifier: MIT
// src/xeno_wrapper.c
// Canonical provider of Vulkan loader entry points for ExynosTools.
// This file unconditionally exports the Vulkan loader entry points
// with the exact signatures declared by the Vulkan headers.

#include <vulkan/vulkan.h>
#include <stddef.h>

/* Ensure this unit provides the global symbols */
#ifndef PROVIDE_VK_GLOBALS
#define PROVIDE_VK_GLOBALS 1
#endif

/* Exported implementations MUST match the Vulkan header signatures exactly. */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    (void)instance;
    (void)pName;
    /* Replace with your real loader/dispatch logic if needed.
       Returning NULL is valid at compile time; runtime resolution may require actual implementation. */
    return (PFN_vkVoidFunction)NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    (void)device;
    (void)pName;
    return (PFN_vkVoidFunction)NULL;
}
