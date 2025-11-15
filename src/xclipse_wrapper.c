// src/xclipse_wrapper.c
// Full drop-in replacement for xclipse_wrapper.c
// - Correct filename (xclipse), C99-compliant, self-contained
// - Forwards instance/device proc lookups safely and avoids unused warnings
// - Uses project logging macros for diagnostics

#include <vulkan/vulkan.h>
#include <stdio.h>
#include "xeno_log.h"

/* Safe wrappers for resolving Vulkan function pointers.
   These forward to vkGetInstanceProcAddr/vkGetDeviceProcAddr when available. */

/* Note: vkGetInstanceProcAddr and vkGetDeviceProcAddr are provided by the Vulkan loader.
   We call them directly; if they are NULL, we log and return NULL. */

static PFN_vkVoidFunction xclipse_get_instance_proc_addr(VkInstance instance, const char* name)
{
    if (!name) return NULL;
    if (!vkGetInstanceProcAddr) {
        XENO_LOGW("xclipse_wrapper: vkGetInstanceProcAddr not available");
        return NULL;
    }
    return vkGetInstanceProcAddr(instance, name);
}

static PFN_vkVoidFunction xclipse_get_device_proc_addr(VkDevice device, const char* name)
{
    if (!name) return NULL;
    if (!vkGetDeviceProcAddr) {
        XENO_LOGW("xclipse_wrapper: vkGetDeviceProcAddr not available");
        return NULL;
    }
    return vkGetDeviceProcAddr(device, name);
}

/* Public helpers to resolve functions by name. Return NULL on failure. */

PFN_vkVoidFunction xclipse_resolve_instance_function(VkInstance instance, const char* name)
{
    return xclipse_get_instance_proc_addr(instance, name);
}

PFN_vkVoidFunction xclipse_resolve_device_function(VkDevice device, const char* name)
{
    return xclipse_get_device_proc_addr(device, name);
}

/* Small convenience that attempts to fetch a well-known function and logs result.
   Useful for initialization code that wants to assert presence of entrypoints. */

int xclipse_query_and_log(VkInstance instance, VkDevice device)
{
    PFN_vkVoidFunction fn;

    fn = xclipse_resolve_instance_function(instance, "vkCreateInstance");
    XENO_LOGI("xclipse_wrapper: vkCreateInstance %s", fn ? "available" : "missing");

    fn = xclipse_resolve_device_function(device, "vkCreateDevice");
    XENO_LOGI("xclipse_wrapper: vkCreateDevice %s", fn ? "available" : "missing");

    return 0;
}

/* No-op init/shutdown exported for link-time symmetry */
int xclipse_wrapper_init(void)
{
    XENO_LOGI("xclipse_wrapper: init");
    return 0;
}

void xclipse_wrapper_shutdown(void)
{
    XENO_LOGI("xclipse_wrapper: shutdown");
}
