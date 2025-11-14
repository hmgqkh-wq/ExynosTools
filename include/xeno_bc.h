/* include/xeno_bc.h
 * SPDX-License-Identifier: MIT
 *
 * Public BC emulator API tuned for Xclipse 940.
 * This header declares the context type and the decode API your sources expect.
 * Place this file in your repo at include/xeno_bc.h so CMake include paths can find it.
 */

#ifndef XENO_BC_H_
#define XENO_BC_H_

#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Force Xclipse 940 optimizations path at compile time if not already defined */
#ifndef XCLIPSE_940_OPTIMIZE
#define XCLIPSE_940_OPTIMIZE 1
#endif

/* BC image formats supported by the emulator */
typedef enum {
    VK_IMAGE_BC1 = 0,
    VK_IMAGE_BC3,
    VK_IMAGE_BC4,
    VK_IMAGE_BC5,
    VK_IMAGE_BC6H,
    VK_IMAGE_BC7
} VkImageBCFormat;

/* Opaque context holding cached pipelines, shader modules and allocators */
typedef struct XenoBCContext XenoBCContext;

/* Create / destroy context
 * - device: VkDevice used to create pipelines and buffers
 * - out_ctx: receives allocated context pointer
 * The caller should set ctx->physical and ctx->queue if they need optimized upload/submit paths.
 */
VkResult xeno_bc_create_context(VkDevice device, XenoBCContext **out_ctx);
void xeno_bc_destroy_context(XenoBCContext *ctx);

/*
 * Decode API
 *
 * This function preserves the emulator/features expected by the codebase and is tuned
 * for Xclipse 940 performance. It supports two input modes:
 *  - host_data/host_size non-NULL: data is staged into an internal host-coherent staging pool
 *  - src_buffer != VK_NULL_HANDLE: use this VkBuffer directly as source (no staging)
 *
 * Parameters:
 *  - cmd: a VkCommandBuffer in RECORDING state where the decode commands will be recorded
 *  - ctx: pointer returned by xeno_bc_create_context
 *  - host_data: pointer to host memory containing BC-compressed bytes (or NULL if using src_buffer)
 *  - host_size: size in bytes of host_data
 *  - src_buffer: optional VkBuffer already containing BC bytes (or VK_NULL_HANDLE)
 *  - dst_rgba: VkImage that will receive decoded RGBA pixels (caller must ensure correct layout, usually VK_IMAGE_LAYOUT_GENERAL)
 *  - format: one of VkImageBCFormat
 *  - extent: VkExtent3D describing the destination image region to decode
 *
 * Return: standard VkResult (VK_SUCCESS on success)
 */
VkResult xeno_bc_decode_image(VkCommandBuffer cmd,
                              XenoBCContext *ctx,
                              const void *host_data,
                              size_t host_size,
                              VkBuffer src_buffer,
                              VkImage dst_rgba,
                              VkImageBCFormat format,
                              VkExtent3D extent);

/* Optional helpers you can implement/consume later (prototypes only) */
/* Create a VkBuffer filled from host memory and return it (caller frees) */
VkResult xeno_bc_create_staging_buffer(XenoBCContext *ctx, const void *data, size_t size, VkBuffer *out_buf, VkDeviceMemory *out_mem);

/* Query context capabilities (e.g., subgroup size) */
uint32_t xeno_bc_get_subgroup_size(const XenoBCContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* XENO_BC_H_ */
