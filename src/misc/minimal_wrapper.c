// src/misc/minimal_wrapper.c
// Full drop-in replacement — includes xeno_log.h and avoids unused-function warnings.

#include <vulkan/vulkan.h>
#include "xeno_log.h"

/* Mark helpers unused to silence warnings when not referenced. */
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

PFN_vkVoidFunction minimal_resolve_device_symbol(VkDevice device, const char *name)
{
    if (!name) return NULL;
    PFN_vkVoidFunction fn = my_vkGetDeviceProcAddr(device, name);
    if (!fn) {
        XENO_LOGD("minimal_wrapper: resolve failed for %s", name);
    }
    return fn;
}
