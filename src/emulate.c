/*
  src/emulate.c
  Entry points for testing decode pipelines.
*/
#include "xeno_bc.h"
#include "logging.h"

VkResult run_decode_test(VkDevice device, VkPhysicalDevice physical, VkQueue queue) {
    struct XenoBCContext *ctx = NULL;
    VkResult r = xeno_bc_create_context(device, physical, queue, &ctx);
    if (r != VK_SUCCESS) {
        logging_error("Failed to create BC context: %d", (int)r);
        return r;
    }
    // In production, record command buffer and call xeno_bc_decode_image.
    xeno_bc_destroy_context(ctx);
    return VK_SUCCESS;
}
