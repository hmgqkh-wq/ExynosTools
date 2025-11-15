// include/hud.h
#ifndef XENO_HUD_H
#define XENO_HUD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include <stdint.h>

/* Minimal HUD context used by the provided hud.c stub and by callers.
   Add fields as needed for a full implementation (pipelines, descriptors, buffers). */

typedef struct XenoHUDContext {
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    uint32_t swapchainImageCount;
    int initialized;         /* nonzero when resources created */
    uint32_t frameCount;     /* total frames rendered */
    float frameTime;         /* last frame time in seconds */
    double lastTime;         /* last timestamp used to compute frameTime */
    /* Placeholders for real resources (populate in a full implementation) */
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
    VkImageView *swapchainImageViews;
} XenoHUDContext;

/* Create and initialize HUD context.
   - device: logical Vulkan device
   - physicalDevice: physical device used for memory queries
   - swapchainFormat / swapchainExtent: info for pipeline creation
   - imageCount / swapchainImageViews: swapchain images for resource creation
   Returns allocated context pointer on success, or NULL on failure. */
XenoHUDContext* xeno_hud_create_context(VkDevice device, VkPhysicalDevice physicalDevice,
                                       VkFormat swapchainFormat, VkExtent2D swapchainExtent,
                                       uint32_t imageCount, VkImageView* swapchainImageViews);

/* Destroy HUD context and free all resources created by create_context. */
void xeno_hud_destroy_context(XenoHUDContext* ctx);

/* Begin HUD frame; called before recording HUD draw commands for a frame.
   Returns VK_SUCCESS when ready, or an error code on failure. */
VkResult xeno_hud_begin_frame(XenoHUDContext* ctx);

/* Record HUD draw commands into cmd for the current imageIndex.
   Should be called while recording the primary command buffer for the frame.
   Returns VK_SUCCESS on success or an error VkResult. */
VkResult xeno_hud_draw(XenoHUDContext* ctx, VkCommandBuffer cmd, uint32_t imageIndex);

/* End HUD frame; called after HUD draw commands and before present. */
void xeno_hud_end_frame(XenoHUDContext* ctx);

/* Update HUD internal FPS timing.
   - currentTime: high-resolution timestamp in seconds (e.g., from clock_gettime or similar) */
void xeno_hud_update_fps(XenoHUDContext* ctx, double currentTime);

#ifdef __cplusplus
}
#endif

#endif /* XENO_HUD_H */
