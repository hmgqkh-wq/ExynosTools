// src/xclipse_wrapper.c
// Full drop-in replacement for xclipse_wrapper.c
// - Includes a proper header with prototypes to avoid missing-prototypes warnings
// - Uses xeno_log.h for logging declarations (keeps them consistent)
// - Resolves instance/device functions safely and exports init/shutdown

#include <vulkan/vulkan.h>
#include <string.h>
#include "xeno_log.h"
#include "xclipse_wrapper.h"

/* Internal helpers: attempt to resolve via loader global entrypoints.
   Using address-of checks to silence pointer-bool conversion warnings. */

static PFN_vkVoidFunction xclipse_get_instance_proc_addr_internal(VkInstance instance, const char* name)
{
    if (!name) return NULL;
    if (&vkGetInstanceProcAddr) {
        return vkGetInstanceProcAddr(instance, name);
    }
    XENO_LOGW("xclipse_get_instance_proc_addr_internal: vkGetInstanceProcAddr not available");
    return NULL;
}

static PFN_vkVoidFunction xclipse_get_device_proc_addr_internal(VkDevice device, const char* name)
{
    if (!name) return NULL;
    if (&vkGetDeviceProcAddr) {
        return vkGetDeviceProcAddr(device, name);
    }
    XENO_LOGW("xclipse_get_device_proc_addr_internal: vkGetDeviceProcAddr not available");
    return NULL;
}

/* Public resolver functions (previously triggered missing-prototype warnings).
   They simply forward to the internal resolvers. */

PFN_vkVoidFunction xclipse_resolve_instance_function(VkInstance instance, const char* name)
{
    return xclipse_get_instance_proc_addr_internal(instance, name);
}

PFN_vkVoidFunction xclipse_resolve_device_function(VkDevice device, const char* name)
{
    return xclipse_get_device_proc_addr_internal(device, name);
}

/* Exported initialization and shutdown (match prototypes in include/xclipse_wrapper.h) */

int xclipse_wrapper_init(void)
{
    XENO_LOGI("xclipse_wrapper: init");
    return 0;
}

void xclipse_wrapper_shutdown(void)
{
    XENO_LOGI("xclipse_wrapper: shutdown");
}
