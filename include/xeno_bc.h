// include/xeno_bc.h
// Minimal public header for Xeno BC context API.
// - Declares struct tag 'struct XenoBCContext' and the public create/destroy API
// - Matches the prototype style used by your build errors/logs
// - Keeps only the essentials so other modules can compile and link

#ifndef XENO_BC_H
#define XENO_BC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include <stdint.h>

/* Opaque context struct tag. Use 'struct XenoBCContext' in C source to match usage. */
struct XenoBCContext;

/* Return codes follow Vulkan VkResult semantics */
VkResult xeno_bc_create_context(VkDevice device,
                                VkPhysicalDevice physical,
                                VkQueue queue,
                                struct XenoBCContext **out_ctx);

/* Destroy and free the context created above. Safe to call with NULL. */
void xeno_bc_destroy_context(struct XenoBCContext *ctx);

/* Optional runtime query helpers (no-op defaults; implement in bc_emulate.c if you need them) */
int xeno_bc_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* XENO_BC_H */
