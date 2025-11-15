// src/misc/minimal_wrapper.c
// Full drop-in replacement for minimal loader wrapper utilities.
// - Marks local helper functions as static and unused to silence warnings
// - Includes required headers
// - Uses project logging macros

#include <vulkan/vulkan.h>
#include "xeno_log.h"

/* Local helpers kept static and explicitly marked unused where appropriate
   to avoid compiler warnings in builds that don't use them. */

#if defined(__GNUC__)
static PFN_vkVoidFunction my_vkGetInstanceProcAddr(VkInstance instance, const char* pname) __attribute__((unused));
static PFN_vkVoidFunction my_vkGetDeviceProcAddr(VkDevice device, const char* pname) __attribute__((unused));
#endif

static PFN_vkVoidFunction my_vkGetInstanceProcAddr(VkInstance instance, const char* pname)
{
    (void)instance;
    if (&vkGetInstanceProcAddr) {
        return vkGetInstanceProcAddr(instance, pname);
    }
    return NULL;
}

static PFN_vkVoidFunction my_vkGetDeviceProcAddr(VkDevice device, const char* pname)
{
    (void)device;
    if (&vkGetDeviceProcAddr) {
        return vkGetDeviceProcAddr(device, pname);
    }
    return NULL;
}

/* Simple exported helper used by the project to resolve one device symbol safely.
   Returns NULL if resolution fails. */
PFN_vkVoidFunction minimal_resolve_device_symbol(VkDevice device, const char *name)
{
    if (!name) return NULL;
    PFN_vkVoidFunction fn = my_vkGetDeviceProcAddr(device, name);
    if (!fn) {
        XENO_LOGD("minimal_wrapper: resolve failed for %s", name);
    }
    return fn;
}
