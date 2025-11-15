// src/xclipse_wrapper.c
#include <vulkan/vulkan.h>
#include "xeno_log.h"
#include <string.h>

static PFN_vkVoidFunction xclipse_get_instance_proc_addr(VkInstance instance, const char* name)
{
    if (!name) return NULL;
    if (&vkGetInstanceProcAddr) {
        return vkGetInstanceProcAddr(instance, name);
    }
    XENO_LOGW("xclipse_wrapper: vkGetInstanceProcAddr not available");
    return NULL;
}

static PFN_vkVoidFunction xclipse_get_device_proc_addr(VkDevice device, const char* name)
{
    if (!name) return NULL;
    if (&vkGetDeviceProcAddr) {
        return vkGetDeviceProcAddr(device, name);
    }
    XENO_LOGW("xclipse_wrapper: vkGetDeviceProcAddr not available");
    return NULL;
}

PFN_vkVoidFunction xclipse_resolve_instance_function(VkInstance instance, const char* name)
{
    return xclipse_get_instance_proc_addr(instance, name);
}

PFN_vkVoidFunction xclipse_resolve_device_function(VkDevice device, const char* name)
{
    return xclipse_get_device_proc_addr(device, name);
}

int xclipse_wrapper_init(void)
{
    XENO_LOGI("xclipse_wrapper: init");
    return 0;
}

void xclipse_wrapper_shutdown(void)
{
    XENO_LOGI("xclipse_wrapper: shutdown");
}
