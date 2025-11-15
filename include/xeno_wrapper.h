// include/xeno_wrapper.h
#ifndef XENO_WRAPPER_H
#define XENO_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include "xeno_bc.h"

/* Public prototypes for wrapper functions used by other TUs */
VkResult xeno_wrapper_create_device(VkPhysicalDevice physicalDevice,
                                    const VkDeviceCreateInfo *pCreateInfo,
                                    const VkAllocationCallbacks *pAllocator,
                                    VkDevice *pDevice);

void xeno_wrapper_begin_render(VkCommandBuffer commandBuffer,
                               const VkRenderPassBeginInfo *pRenderPassBeginInfo,
                               VkSubpassContents contents);

void xeno_wrapper_destroy(struct XenoBCContext *maybe_ctx);

#ifdef __cplusplus
}
#endif

#endif /* XENO_WRAPPER_H */
