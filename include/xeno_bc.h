#ifndef XENO_BC_H_
#define XENO_BC_H_

#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VkImageBCFormat {
    VK_IMAGE_BC1 = 0,
    VK_IMAGE_BC2,
    VK_IMAGE_BC3,
    VK_IMAGE_BC4,
    VK_IMAGE_BC5,
    VK_IMAGE_BC6H,
    VK_IMAGE_BC7
} VkImageBCFormat;

struct XenoBCContext;

/* Create a decoding context tuned for Xclipse 940.
 * - device: logical Vulkan device
 * - physical: physical device handle
 * - queue: compute queue used for dispatches
 * - out_ctx: receives allocated context pointer
 *
 * Returns VkResult (VK_SUCCESS on success).
 */
VkResult xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical, VkQueue queue, struct XenoBCContext **out_ctx);

/* Destroy context and free resources */
void xeno_bc_destroy_context(struct XenoBCContext *ctx);

/* Decode a single image using BC pipeline:
 * - cmd: command buffer to record the decode (must be in recording state)
 * - ctx: decoding context
 * - host_data: optional host pointer containing compressed BC payload (if provided, it will be staged)
 * - host_size: size of host_data in bytes
 * - src_buffer: optional GPU buffer already containing compressed data (use VK_WHOLE_SIZE to mean full)
 * - dst_view: destination VkImageView for storage image writes (image must be in GENERAL layout)
 * - format: BC format enum above
 * - extent: width/height/depth of target image
 *
 * Returns VkResult.
 */
VkResult xeno_bc_decode_image(VkCommandBuffer cmd, struct XenoBCContext *ctx, const void *host_data, size_t host_size, VkBuffer src_buffer, VkImageView dst_view, VkImageBCFormat format, VkExtent3D extent);

/* Query helper: returns workgroup sizes optimized for Xclipse 940 */
void xeno_bc_get_optimal_local_size(uint32_t *local_x, uint32_t *local_y);

#ifdef __cplusplus
}
#endif

#endif /* XENO_BC_H_ */
