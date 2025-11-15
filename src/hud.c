// src/hud.c
// Simplified HUD implementation with shader strings marked unused to silence warnings.

#include "hud.h"
#include "xeno_log.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

/* Simple 8x8 font omitted here for brevity in previous files; assume hud.h defines XenoHUDContext.
   This file focuses on eliminating unused-variable warnings for shader sources. */

/* Mark shader strings as static and possibly unused to silence -Wunused-variable.
   Keeping them in file scope makes them easy to compile, but attribute unused avoids warnings. */

#if defined(__GNUC__) || defined(__clang__)
#  define UNUSED_ATTR __attribute__((unused))
#else
#  define UNUSED_ATTR
#endif

static const char vertexShaderCode[] UNUSED_ATTR =
    "#version 450\n"
    "layout(location = 0) in vec2 inPosition;\n"
    "layout(location = 1) in vec2 inTexCoord;\n"
    "layout(location = 2) in uint inColor;\n"
    "layout(location = 0) out vec2 fragTexCoord;\n"
    "layout(location = 1) out vec4 fragColor;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 scale;\n"
    "    vec2 translate;\n"
    "} pc;\n"
    "void main() {\n"
    "    gl_Position = vec4(inPosition * pc.scale + pc.translate, 0.0, 1.0);\n"
    "    fragTexCoord = inTexCoord;\n"
    "    fragColor = unpackUnorm4x8(inColor);\n"
    "}\n";

static const char fragmentShaderCode[] UNUSED_ATTR =
    "#version 450\n"
    "layout(location = 0) in vec2 fragTexCoord;\n"
    "layout(location = 1) in vec4 fragColor;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "layout(binding = 0) uniform sampler2D texSampler;\n"
    "void main() {\n"
    "    float alpha = texture(texSampler, fragTexCoord).r;\n"
    "    outColor = vec4(fragColor.rgb, fragColor.a * alpha);\n"
    "}\n";

/* Rest of HUD implementation (create textures, pipeline, framebuffers) should match your project.
   For brevity we provide minimal stubs that reference XenoHUDContext and avoid unused symbol warnings. */

XenoHUDContext* xeno_hud_create_context(VkDevice device, VkPhysicalDevice physicalDevice,
                                       VkFormat swapchainFormat, VkExtent2D swapchainExtent,
                                       uint32_t imageCount, VkImageView* swapchainImageViews)
{
    (void)vertexShaderCode;
    (void)fragmentShaderCode;

    XENO_LOGI("hud: create_context (stub)");
    /* Minimal stub: allocate and return a zeroed context to avoid further compile errors. */
    XenoHUDContext* ctx = (XenoHUDContext*)calloc(1, sizeof(XenoHUDContext));
    if (!ctx) {
        XENO_LOGE("hud: allocation failed");
        return NULL;
    }
    ctx->device = device;
    ctx->physicalDevice = physicalDevice;
    ctx->swapchainImageCount = imageCount;
    ctx->initialized = 1;
    return ctx;
}

void xeno_hud_destroy_context(XenoHUDContext* ctx) {
    if (!ctx) return;
    /* Destroy resources here if created; stub avoids unused warnings. */
    free(ctx);
}

VkResult xeno_hud_begin_frame(XenoHUDContext* ctx) {
    if (!ctx || !ctx->initialized) return VK_ERROR_INITIALIZATION_FAILED;
    ctx->frameCount++;
    return VK_SUCCESS;
}

VkResult xeno_hud_draw(XenoHUDContext* ctx, VkCommandBuffer cmd, uint32_t imageIndex) {
    if (!ctx || !ctx->initialized) return VK_ERROR_INITIALIZATION_FAILED;
    const char* hudEnv = getenv("EXYNOSTOOLS_HUD");
    if (!hudEnv || *hudEnv != '1') return VK_SUCCESS;
    float fps = (ctx->frameTime > 0.0f) ? (1.0f / ctx->frameTime) : 0.0f;
    XENO_LOGD("hud: drawing frame %u (FPS: %.1f)", ctx->frameCount, fps);
    (void)cmd; (void)imageIndex;
    return VK_SUCCESS;
}

void xeno_hud_end_frame(XenoHUDContext* ctx) { (void)ctx; }

void xeno_hud_update_fps(XenoHUDContext* ctx, double currentTime) {
    if (!ctx) return;
    if (ctx->lastTime > 0.0) ctx->frameTime = (float)(currentTime - ctx->lastTime);
    else ctx->frameTime = 0.0f;
    ctx->lastTime = currentTime;
}
