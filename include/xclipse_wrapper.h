// include/xclipse_wrapper.h
#ifndef XCLIPSE_WRAPPER_H
#define XCLIPSE_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include "xeno_log.h"

/* Public resolver helpers for instance/device functions */
PFN_vkVoidFunction xclipse_resolve_instance_function(VkInstance instance, const char* name);
PFN_vkVoidFunction xclipse_resolve_device_function(VkDevice device, const char* name);

/* Initialization / shutdown for the wrapper (exported) */
int xclipse_wrapper_init(void);
void xclipse_wrapper_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* XCLIPSE_WRAPPER_H */
