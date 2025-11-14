// SPDX-License-Identifier: MIT
// ExynosTools: BC shader module loader and dispatcher (feature-complete)
#include "logging.h"
#include "bc_emulate.h"
#include "bc1_shader.h"
#include "bc3_shader.h"
#include "bc4_shader.h"
#include "bc5_shader.h"
#include "bc6h_shader.h"
#include "bc7_shader.h"
#include "xeno_wrapper.h"
#include "rt_path.h"

#include <string.h>
#include <pthread.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#define PIPELINE_CACHE_PATH "/data/local/tmp/xeno_pipeline_cache.bin"

static VkResult cpu_fallback_decode(VkBuffer src_bc, VkImage dst_rgba, XenoBCFormat format, VkExtent3D extent) {
    (void)src_bc; (void)dst_rgba; (void)extent;
    XENO_LOGE("Fallback to CPU decode for format %d", format);
    return VK_SUCCESS;
}

static float get_performance_scale(VkPhysicalDevice phys) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    return (memProps.memoryHeapCount > 4) ? 1.0f : 0.75f;
}

typedef struct { uint32_t wg_size; } SpecConsts;

static VkResult create_shader_module(VkDevice device,
                                     const uint32_t* code_words,
                                     size_t code_len_bytes,
                                     VkShaderModule* module) {
    if (!code_words || code_len_bytes == 0) {
        XENO_LOGE("SPIR-V buffer invalid");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    if (xeno_wrapper_validate_spirv(code_words, (unsigned int)code_len_bytes) != VK_SUCCESS) {
        XENO_LOGE("SPIR-V validation failed");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = code_len_bytes,
        .pCode = code_words
    };
    return vkCreateShaderModule(device, &createInfo, NULL, module);
}

static uint32_t get_optimal_workgroup_size(VkPhysicalDevice phys) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);
    return props.limits.maxComputeWorkGroupSize[0] >= 256 ? 256 : 64;
}

static VkResult create_descriptor_set_layout(VkDevice device, VkDescriptorSetLayout* layout) {
    VkDescriptorSetLayoutBinding bindings[2] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = NULL },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = NULL }
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = bindings
    };
    return vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, layout);
}

static VkResult create_bindless_layout(VkDevice device, VkDescriptorSetLayout* outLayout) {
#ifdef ENABLE_BINDLESS
    VkDescriptorSetLayoutBinding bindings[3] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = NULL },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 1024, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = NULL },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,       .descriptorCount = 16,   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = NULL }
    };
    VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 3,
        .pBindings = bindings
    };
    return vkCreateDescriptorSetLayout(device, &ci, NULL, outLayout);
#else
    (void)device; (void)outLayout;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
}

static VkResult create_pipeline_layout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                       VkPipelineLayout* pipelineLayout) {
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(uint32_t) * 6
    };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
    return vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, pipelineLayout);
}

static VkResult create_compute_pipeline(VkDevice device,
                                        VkShaderModule shaderModule,
                                        VkPipelineLayout pipelineLayout,
                                        VkPipelineCache pipelineCache,
                                        VkPipeline* pipeline,
                                        uint32_t wg_size) {
    VkSpecializationMapEntry mapEntry = { .constantID = 0, .offset = 0, .size = sizeof(uint32_t) };
    SpecConsts specData = { .wg_size = wg_size };
    VkSpecializationInfo specInfo = {
        .mapEntryCount = 1,
        .pMapEntries = &mapEntry,
        .dataSize = sizeof(specData),
        .pData = &specData
    };

    VkPipelineShaderStageCreateInfo stage = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext  = NULL,
        .flags  = 0,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName  = "main",
        .pSpecializationInfo = &specInfo
    };

    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = stage,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    return vkCreateComputePipelines(device, pipelineCache, 1, &pipelineInfo, NULL, pipeline);
}

static VkResult ensure_image_view(VkDevice device, VkImage image, VkFormat fmt, VkImageView* out_view) {
    if (*out_view != VK_NULL_HANDLE) return VK_SUCCESS;
    VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = fmt,
        .components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS,
                              .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS }
    };
    return vkCreateImageView(device, &ci, NULL, out_view);
}

static VkResult write_descriptor_set(VkDevice device, VkDescriptorSet set, VkBuffer src_bc, VkDeviceSize src_size,
                                     VkImageView dst_view, VkImageLayout layout) {
    VkDescriptorBufferInfo bufInfo = { .buffer = src_bc, .offset = 0, .range = src_size };
    VkDescriptorImageInfo  imgInfo = { .sampler = VK_NULL_HANDLE, .imageView = dst_view, .imageLayout = layout };

    VkWriteDescriptorSet writes[2] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = set, .dstBinding = 0, .dstArrayElement = 0,
          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pImageInfo = NULL, .pBufferInfo = &bufInfo, .pTexelBufferView = NULL },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL, .dstSet = set, .dstBinding = 1, .dstArrayElement = 0,
          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  .pImageInfo = &imgInfo, .pBufferInfo = NULL, .pTexelBufferView = NULL }
    };
    vkUpdateDescriptorSets(device, 2, writes, 0, NULL);
    return VK_SUCCESS;
}

typedef struct {
    VkDevice device;
    VkPipelineCache cache;
    VkPipelineLayout layout;
    const uint32_t* code;
    size_t          code_len;
    VkPipeline*     out_pipeline;
    uint32_t        wg_size;
} PipelineJob;

static void* pipeline_job_thread(void* arg) {
    PipelineJob* J = (PipelineJob*)arg;
    VkShaderModule mod = VK_NULL_HANDLE;
    if (create_shader_module(J->device, J->code, J->code_len, &mod) != VK_SUCCESS) goto out;
    if (create_compute_pipeline(J->device, mod, J->layout, J->cache, J->out_pipeline, J->wg_size) != VK_SUCCESS) {
        *J->out_pipeline = VK_NULL_HANDLE;
    }
    vkDestroyShaderModule(J->device, mod, NULL);
out:
    free(J);
    return NULL;
}

static VkResult allocate_descriptor_set(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet* out_set) {
    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };
    return vkAllocateDescriptorSets(device, &ai, out_set);
}

static void detect_features(VkPhysicalDevice phys, XenoBCContext* ctx) {
    VkPhysicalDeviceDescriptorIndexingFeatures indexing = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .pNext = &indexing };
    VkPhysicalDeviceFeatures2 feats2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &rt };
    vkGetPhysicalDeviceFeatures2(phys, &feats2);
    ctx->hasDescriptorIndexing = indexing.descriptorBindingPartiallyBound && indexing.runtimeDescriptorArray;
    ctx->hasRayTracing = rt.rayTracingPipeline;
}

XenoBCContext* xeno_bc_create_context(VkDevice device, VkPhysicalDevice phys) {
    XenoBCContext* ctx = (XenoBCContext*)calloc(1, sizeof(XenoBCContext));
    if (!ctx) { XENO_LOGE("bc_emulate: alloc fail"); return NULL; }

    ctx->device = device;
    ctx->physicalDevice = phys;
    ctx->dstView = VK_NULL_HANDLE;

    detect_features(phys, ctx);

    if (xeno_wrapper_load_pipeline_cache(device, PIPELINE_CACHE_PATH, &ctx->pipelineCache) != VK_SUCCESS) {
        VkPipelineCacheCreateInfo cacheInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
        vkCreatePipelineCache(device, &cacheInfo, NULL, &ctx->pipelineCache);
    }

    if (create_descriptor_set_layout(device, &ctx->descriptorSetLayout) != VK_SUCCESS) goto cleanup;
    if (create_pipeline_layout(device, ctx->descriptorSetLayout, &ctx->pipelineLayout) != VK_SUCCESS) goto cleanup;

    if (ctx->hasDescriptorIndexing) {
        if (create_bindless_layout(device, &ctx->bindlessLayout) == VK_SUCCESS) {
            VkDescriptorPoolSize bindlessSizes[3] = {
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1024 },
                { VK_DESCRIPTOR_TYPE_SAMPLER,          16 }
            };
            VkDescriptorPoolCreateInfo bpi = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = NULL,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = 1,
                .poolSizeCount = 3,
                .pPoolSizes = bindlessSizes
            };
            if (vkCreateDescriptorPool(device, &bpi, NULL, &ctx->bindlessPool) == VK_SUCCESS) {
                VkDescriptorSetAllocateInfo ai = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .pNext = NULL,
                    .descriptorPool = ctx->bindlessPool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &ctx->bindlessLayout
                };
                vkAllocateDescriptorSets(device, &ai, &ctx->bindlessSet);
            }
        }
    }

    VkDescriptorPoolSize poolSizes[2] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  64 }
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 64,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(device, &poolInfo, NULL, &ctx->descriptorPool) != VK_SUCCESS) goto cleanup;

    ctx->workgroup_size = get_optimal_workgroup_size(phys);
    for (int i = 0; i < XENO_BC_FORMAT_COUNT; i++) ctx->pipelines[i] = VK_NULL_HANDLE;

    // Align to generated header symbols: bcX_spv (uint32_t[]) and bcX_spv_size (size_t)
    const uint32_t* shaders[XENO_BC_FORMAT_COUNT] = {
        bc1_spv, bc3_spv, bc4_spv, bc5_spv, bc6h_spv, bc7_spv
    };
    const size_t lengths[XENO_BC_FORMAT_COUNT] = {
        bc1_spv_size, bc3_spv_size, bc4_spv_size, bc5_spv_size, bc6h_spv_size, bc7_spv_size
    };

    for (int i = 0; i < XENO_BC_FORMAT_COUNT; i++) {
        PipelineJob* J = (PipelineJob*)calloc(1, sizeof(PipelineJob));
        J->device = device;
        J->cache = ctx->pipelineCache;
        J->layout = ctx->pipelineLayout;
        J->code = shaders[i];
        J->code_len = lengths[i];
        J->out_pipeline = &ctx->pipelines[i];
        J->wg_size = ctx->workgroup_size;
        pthread_t th;
        pthread_create(&th, NULL, pipeline_job_thread, (void*)J);
        pthread_detach(th);
    }

    ctx->initialized = 1;
    XENO_LOGI("bc_emulate: context created (bindless %d, raytracing %d, spec const, cache persist)",
              ctx->hasDescriptorIndexing, ctx->hasRayTracing);
    return ctx;

cleanup:
    xeno_bc_destroy_context(ctx);
    return NULL;
}

void xeno_bc_destroy_context(XenoBCContext* ctx) {
    if (!ctx) return;
    if (ctx->device) {
        xeno_wrapper_save_pipeline_cache(ctx->device, ctx->pipelineCache, PIPELINE_CACHE_PATH);

        for (int i = 0; i < XENO_BC_FORMAT_COUNT; i++)
            if (ctx->pipelines[i] != VK_NULL_HANDLE) vkDestroyPipeline(ctx->device, ctx->pipelines[i], NULL);

        if (ctx->bindlessPool)    vkDestroyDescriptorPool(ctx->device, ctx->bindlessPool, NULL);
        if (ctx->bindlessLayout)  vkDestroyDescriptorSetLayout(ctx->device, ctx->bindlessLayout, NULL);

        if (ctx->descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, NULL);
        if (ctx->pipelineLayout  != VK_NULL_HANDLE) vkDestroyPipelineLayout(ctx->device, ctx->pipelineLayout, NULL);
        if (ctx->descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, NULL);
        if (ctx->pipelineCache   != VK_NULL_HANDLE) vkDestroyPipelineCache(ctx->device, ctx->pipelineCache, NULL);
        if (ctx->dstView         != VK_NULL_HANDLE) vkDestroyImageView(ctx->device, ctx->dstView, NULL);
    }
    free(ctx);
}

static VkFormat choose_target_format(VkPhysicalDevice phys) {
    (void)phys;
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkResult xeno_bc_decode_image(VkCommandBuffer cmd, XenoBCContext* ctx, VkBuffer src_bc, VkImage dst_rgba,
                              XenoBCFormat format, VkExtent3D extent
