#ifndef XENO_BC_H_
#define XENO_BC_H_

#include <vulkan/vulkan.h>

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
VkResult xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical, VkQueue queue, struct XenoBCContext **out_ctx);
void xeno_bc_destroy_context(struct XenoBCContext *ctx);
VkResult xeno_bc_decode_image(VkCommandBuffer cmd, struct XenoBCContext *ctx, const void *host_data, size_t host_size, VkBuffer src_buffer, VkImageView dst_view, VkImageBCFormat format, VkExtent3D extent);

#endif // XENO_BC_H_
