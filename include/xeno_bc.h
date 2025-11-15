// include/xeno_bc.h
#ifndef XENO_BC_H
#define XENO_BC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>

struct XenoBCContext;

VkResult xeno_bc_create_context(VkDevice device,
                                VkPhysicalDevice physical,
                                VkQueue queue,
                                struct XenoBCContext **out_ctx);

void xeno_bc_destroy_context(struct XenoBCContext *ctx);

int xeno_bc_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* XENO_BC_H */
