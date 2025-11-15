// src/hud.c
#include "hud.h"
#include "logging.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

/* Simple 8x8 bitmap font data (embedded) */
static const unsigned char font_bitmap[128][8] = {
    [32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    [48] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    [49] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    [50] = {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00},
    [51] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    [52] = {0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0x00},
    [53] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    [54] = {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00},
    [55] = {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00},
    [56] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    [57] = {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00},
    [65] = {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00},
    [66] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    [67] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
    [68] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    [69] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    [70] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    [71] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00},
    [72] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    [73] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00},
    [74] = {0x3E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00},
    [75] = {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},
    [76] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    [77] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    [78] = {0x66,0x76,0x7E,0x6E,0x66,0x66,0x66,0x00},
    [79] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    [80] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    [81] = {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00},
    [82] = {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00},
    [83] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},
    [84] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    [85] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    [86] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    [87] = {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    [88] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    [89] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    [90] = {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    [58] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    [46] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}
};

static VkResult create_font_texture(XenoHUDContext* ctx) {
    const uint32_t fontWidth = 128u * 8u;
    const uint32_t fontHeight = 8u;
    const uint32_t imageSize = fontWidth * fontHeight;

    unsigned char* fontData = (unsigned char*)malloc((size_t)imageSize);
    if (!fontData) {
        XENO_LOGE("hud: failed to allocate font data");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (int c = 0; c < 128; ++c) {
        for (int y = 0; y < 8; ++y) {
            unsigned char row = font_bitmap[c][y];
            for (int x = 0; x < 8; ++x) {
                uint32_t pixelIndex = (uint32_t)y * fontWidth + (uint32_t)c * 8u + (uint32_t)x;
                fontData[pixelIndex] = (row & (0x80u >> x)) ? 255u : 0u;
            }
        }
    }

    VkResult result;

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = { .width = fontWidth, .height = fontHeight, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    result = vkCreateImage(ctx->device, &imageInfo, NULL, &ctx->fontImage);
    if (result != VK_SUCCESS) {
        free(fontData);
        return result;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(ctx->device, ctx->fontImage, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(ctx->physicalDevice, &memProps);

    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        uint32_t mask = 1u << i;
        if ((memReqs.memoryTypeBits & mask) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }

    if (memoryTypeIndex == UINT32_MAX) {
        free(fontData);
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    result = vkAllocateMemory(ctx->device, &allocInfo, NULL, &ctx->fontImageMemory);
    if (result != VK_SUCCESS) {
        free(fontData);
        return result;
    }

    result = vkBindImageMemory(ctx->device, ctx->fontImage, ctx->fontImageMemory, 0);
    if (result != VK_SUCCESS) {
        free(fontData);
        return result;
    }

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx->fontImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    result = vkCreateImageView(ctx->device, &viewInfo, NULL, &ctx->fontImageView);
    free(fontData);

    return result;
}

static VkResult create_render_pass(XenoHUDContext* ctx, VkFormat swapchainFormat) {
    if (!ctx) return VK_ERROR_INITIALIZATION_FAILED;

    VkAttachmentDescription colorAttachment = {
        .format = swapchainFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    return vkCreateRenderPass(ctx->device, &renderPassInfo, NULL, &ctx->renderPass);
}

static VkResult create_graphics_pipeline(XenoHUDContext* ctx, VkExtent2D swapchainExtent) {
    if (!ctx) return VK_ERROR_INITIALIZATION_FAILED;

    const char* vertexShaderCode =
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

    const char* fragmentShaderCode =
        "#version 450\n"
        "layout(location = 0) in vec2 fragTexCoord;\n"
        "layout(location = 1) in vec4 fragColor;\n"
        "layout(location = 0) out vec4 outColor;\n"
        "layout(binding = 0) uniform sampler2D texSampler;\n"
        "void main() {\n"
        "    float alpha = texture(texSampler, fragTexCoord).r;\n"
        "    outColor = vec4(fragColor.rgb, fragColor.a * alpha);\n"
        "}\n";

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &samplerLayoutBinding
    };

    VkResult result = vkCreateDescriptorSetLayout(ctx->device, &layoutInfo, NULL, &ctx->descriptorSetLayout);
    if (result != VK_SUCCESS) return result;

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float) * 4
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    result = vkCreatePipelineLayout(ctx->device, &pipelineLayoutInfo, NULL, &ctx->pipelineLayout);
    if (result != VK_SUCCESS) return result;

    XENO_LOGI("hud: graphics pipeline creation simplified for demo");
    return VK_SUCCESS;
}

XenoHUDContext* xeno_hud_create_context(VkDevice device, VkPhysicalDevice physicalDevice,
                                       VkFormat swapchainFormat, VkExtent2D swapchainExtent,
                                       uint32_t imageCount, VkImageView* swapchainImageViews)
{
    XenoHUDContext* ctx = (XenoHUDContext*)calloc(1, sizeof(XenoHUDContext));
    if (!ctx) {
        XENO_LOGE("hud: failed to allocate context");
        return NULL;
    }

    ctx->device = device;
    ctx->physicalDevice = physicalDevice;
    ctx->swapchainImageCount = imageCount;
    ctx->frameCount = 0;
    ctx->lastTime = 0.0;
    ctx->frameTime = 0.0f;
    ctx->initialized = 0;

    VkResult result;

    result = create_font_texture(ctx);
    if (result != VK_SUCCESS) {
        XENO_LOGE("hud: failed to create font texture: %d", result);
        goto cleanup;
    }

    result = create_render_pass(ctx, swapchainFormat);
    if (result != VK_SUCCESS) {
        XENO_LOGE("hud: failed to create render pass: %d", result);
        goto cleanup;
    }

    result = create_graphics_pipeline(ctx, swapchainExtent);
    if (result != VK_SUCCESS) {
        XENO_LOGE("hud: failed to create graphics pipeline: %d", result);
        goto cleanup;
    }

    ctx->framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * imageCount);
    if (!ctx->framebuffers) {
        XENO_LOGE("hud: failed to allocate framebuffers");
        goto cleanup;
    }

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx->renderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews[i],
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
            .layers = 1
        };

        result = vkCreateFramebuffer(ctx->device, &framebufferInfo, NULL, &ctx->framebuffers[i]);
        if (result != VK_SUCCESS) {
            XENO_LOGE("hud: failed to create framebuffer %u: %d", i, result);
            goto cleanup;
        }
    }

    ctx->initialized = 1;
    XENO_LOGI("hud: context created successfully");
    return ctx;

cleanup:
    xeno_hud_destroy_context(ctx);
    return NULL;
}

void xeno_hud_destroy_context(XenoHUDContext* ctx) {
    if (!ctx) return;

    if (ctx->device) {
        if (ctx->framebuffers) {
            for (uint32_t i = 0; i < ctx->swapchainImageCount; ++i) {
                if (ctx->framebuffers[i] != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(ctx->device, ctx->framebuffers[i], NULL);
                }
            }
            free(ctx->framebuffers);
            ctx->framebuffers = NULL;
        }

        if (ctx->pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx->device, ctx->pipeline, NULL);
            ctx->pipeline = VK_NULL_HANDLE;
        }

        if (ctx->pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx->device, ctx->pipelineLayout, NULL);
            ctx->pipelineLayout = VK_NULL_HANDLE;
        }

        if (ctx->descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, NULL);
            ctx->descriptorSetLayout = VK_NULL_HANDLE;
        }

        if (ctx->descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, NULL);
            ctx->descriptorPool = VK_NULL_HANDLE;
        }

        if (ctx->renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(ctx->device, ctx->renderPass, NULL);
            ctx->renderPass = VK_NULL_HANDLE;
        }

        if (ctx->fontImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx->device, ctx->fontImageView, NULL);
            ctx->fontImageView = VK_NULL_HANDLE;
        }

        if (ctx->fontImage != VK_NULL_HANDLE) {
            vkDestroyImage(ctx->device, ctx->fontImage, NULL);
            ctx->fontImage = VK_NULL_HANDLE;
        }

        if (ctx->fontImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx->device, ctx->fontImageMemory, NULL);
            ctx->fontImageMemory = VK_NULL_HANDLE;
        }

        if (ctx->fontSampler != VK_NULL_HANDLE) {
            vkDestroySampler(ctx->device, ctx->fontSampler, NULL);
            ctx->fontSampler = VK_NULL_HANDLE;
        }

        if (ctx->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx->device, ctx->vertexBuffer, NULL);
            ctx->vertexBuffer = VK_NULL_HANDLE;
        }

        if (ctx->vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx->device, ctx->vertexBufferMemory, NULL);
            ctx->vertexBufferMemory = VK_NULL_HANDLE;
        }

        if (ctx->indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx->device, ctx->indexBuffer, NULL);
            ctx->indexBuffer = VK_NULL_HANDLE;
        }

        if (ctx->indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx->device, ctx->indexBufferMemory, NULL);
            ctx->indexBufferMemory = VK_NULL_HANDLE;
        }
    }

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
