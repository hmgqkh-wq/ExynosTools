// SPDX-License-Identifier: MIT
// src/xeno_wrapper.c
// Canonical provider of Vulkan loader entry points for ExynosTools.
// This file intentionally exports the public Vulkan entry points exactly once.

#include <vulkan/vulkan.h>
#include <stddef.h>

/*
 * This file must be compiled with PROVIDE_VK_GLOBALS defined.
 * If you paste this file into your repo and use the provided CMake snippet,
 * PROVIDE_VK_GLOBALS will be defined for this file automatically.
 */

/* Minimal exported implementations — replace with your real loader/dispatch logic. */
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    (void)instance;
    (void)pName;
    /* The loader/ICD dispatch should be implemented here. Return NULL by default. */
    return NULL;
}

PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    (void)device;
    (void)pName;
    return NULL;
}

/* Optional: provide other exported helpers or initializers here. */
