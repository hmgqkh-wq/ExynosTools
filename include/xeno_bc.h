/* include/xeno_bc.h
 * SPDX-License-Identifier: MIT
 *
 * Public BC emulator API tuned for Xclipse 940.
 */

#ifndef XENO_BC_H_
#define XENO_BC_H_

#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XCLIPSE_940_OPTIMIZE
#define XCLIPSE_940_OPTIMIZE 1
#endif

typedef enum {
    VK_IMAGE_BC1 = 0,
    VK_IMAGE_BC3,
    VK_IMAGE_BC4,
    VK_IMAGE_BC5,
    VK_IMAGE_BC6H,
    VK_IMAGE_BC7
} VkImageBCFormat;

typedef struct XenoBCContext XenoBCContext;

VkResult xeno_bc_create_context(VkDevice device, XenoBCContext **out_ctx);
void xeno_bc_destroy_context(XenoBCContext *ctx);

/* Decode API:
 * - host_data/host_size non-NULL: stage host data into internal staging pool
 * - src_buffer may be provided instead (no staging)
 * - dst_view: VkImageView of destination image (preferred) or VK_NULL_HANDLE
 *   if you prefer to manage views externally; if VK_NULL_HANDLE, caller must bind dst image separately.
 */
VkResult xeno_bc_decode_image(VkCommandBuffer cmd,
                              XenoBCContext *ctx,
                              const void *host_data,
                              size_t host_size,
                              VkBuffer src_buffer,
                              VkImageView dst_view,
                              VkImageBCFormat format,
                              VkExtent3D extent);

/* Optional helpers */
VkResult xeno_bc_create_staging_buffer(XenoBCContext *ctx, const void *data, size_t size, VkBuffer *out_buf, VkDeviceMemory *out_mem);
uint32_t xeno_bc_get_subgroup_size(const XenoBCContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* XENO_BC_H_ */
