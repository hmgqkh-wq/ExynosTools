// src/bc_emulate.c
// High-performance BC decode pipeline, tuned for Xclipse 940.
// Requires generated headers (bc1_shader.h ... bc7_shader.h) to be present in include path.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <vulkan/vulkan.h>

#include "xeno_bc.h"
#include "logging.h"
#include "xeno_log.h"

/* Expect generated headers to exist. If they don't, build will fail loudly
   so you catch generator issues early. This is deliberate for highest performance. */
#include "bc1_shader.h"
#include "bc2_shader.h"
#include "bc3_shader.h"
#include "bc4_shader.h"
#include "bc5_shader.h"
#include "bc6h_shader.h"
#include "bc7_shader.h"

/* Xclipse 940 tuned local sizes; exposed as specialization constants */
#ifndef XCLIPSE_LOCAL_X
#define XCLIPSE_LOCAL_X 16u
#endif
#ifndef XCLIPSE_LOCAL_Y
#define XCLIPSE_LOCAL_Y 8u
#endif

/* Staging pool size - should be tuned per device */
#ifndef EXYNOSTOOLS_STAGING_POOL_SIZE
#define EXYNOSTOOLS_STAGING_POOL_SIZE (1 << 20) /* 1 MiB by default; tune in CMake if needed */
#endif

#define BINDING_SRC_BUFFER 0
#define BINDING_DST_IMAGE  1

#define LOG_E(...) logging_error(__VA_ARGS__)
#define LOG_I(...) logging_info(__VA_ARGS__)

/* Minimal, performance-oriented context */
struct XenoBCContext {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;
    uint32_t queue_family_index;

    /* shader modules - created once during init */
    VkShaderModule bcModules[7]; /* bc1..bc7 mapped indices 0..6 */

    /* pipelines */
    VkPipeline bcPipelines[7];

    /* layout / descriptor */
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;

    /* staging pool (single buffer, ring-style) */
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    size_t stagingSize;
    _Atomic size_t staging_head; /* atomic byte offset head */

    /* descriptor ring state */
    _Atomic unsigned desc_ring_head;
    uint32_t desc_ring_max;

    /* bookkeeping */
    VkPhysicalDeviceProperties physProps;
    uint32_t subgroup_size;
};

/* Map BC enum to module/pipeline index */
static inline int bc_format_index(VkImageBCFormat f) {
    switch (f) {
    case VK_IMAGE_BC1: return 0;
    case VK_IMAGE_BC2: return 1;
    case VK_IMAGE_BC3: return 2;
    case VK_IMAGE_BC4: return 3;
    case VK_IMAGE_BC5: return 4;
    case VK_IMAGE_BC6H: return 5;
    case VK_IMAGE_BC7: return 6;
    default: return -1;
    }
}

/* Helper: create shader module from SPIR-V words */
static VkResult create_shader_module(VkDevice device, const uint32_t *words, size_t bytes, VkShaderModule *out)
{
    if (!words || bytes == 0) { *out = VK_NULL_HANDLE; return VK_SUCCESS; }
    VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                    .codeSize = bytes,
                                    .pCode = words };
    return vkCreateShaderModule(device, &ci, NULL, out);
}

/* Helper: create compute pipeline with two specialization constants (local_x, local_y) */
static VkResult create_pipeline_for_module(struct XenoBCContext *ctx, VkShaderModule module, VkPipeline *out)
{
    if (!module) { *out = VK_NULL_HANDLE; return VK_SUCCESS; }

    VkSpecializationMapEntry entries[2];
    entries[0].constantID = 0; entries[0].offset = 0; entries[0].size = sizeof(uint32_t);
    entries[1].constantID = 1; entries[1].offset = sizeof(uint32_t); entries[1].size = sizeof(uint32_t);
    uint32_t spec_data[2] = { XCLIPSE_LOCAL_X, XCLIPSE_LOCAL_Y };

    VkSpecializationInfo spec = { .mapEntryCount = 2, .pMapEntries = entries, .dataSize = sizeof(spec_data), .pData = spec_data };

    VkPipelineShaderStageCreateInfo stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = &spec
    };

    VkComputePipelineCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                         .stage = stage,
                                         .layout = ctx->pipelineLayout };

    return vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &cpci, NULL, out);
}

/* Descriptor layout and pipeline layout creation - single set layout covers buffer + image */
static VkResult create_layouts(struct XenoBCContext *ctx)
{
    VkDescriptorSetLayoutBinding b[2];
    b[0].binding = BINDING_SRC_BUFFER; b[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b[0].descriptorCount = 1; b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; b[0].pImmutableSamplers = NULL;

    b[1].binding = BINDING_DST_IMAGE; b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    b[1].descriptorCount = 1; b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT; b[1].pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo dslci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                              .bindingCount = 2, .pBindings = b };
    VkResult r = vkCreateDescriptorSetLayout(ctx->device, &dslci, NULL, &ctx->descriptorSetLayout);
    if (r != VK_SUCCESS) return r;

    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(uint32_t) * 4 };

    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                        .setLayoutCount = 1,
                                        .pSetLayouts = &ctx->descriptorSetLayout,
                                        .pushConstantRangeCount = 1,
                                        .pPushConstantRanges = &pcr };
    return vkCreatePipelineLayout(ctx->device, &plci, NULL, &ctx->pipelineLayout);
}

/* Descriptor pool creation - large pool to avoid runtime allocations */
static VkResult create_descriptor_pool(struct XenoBCContext *ctx)
{
    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; poolSizes[0].descriptorCount = 1024;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;  poolSizes[1].descriptorCount = 1024;

    VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                        .maxSets = 1024,
                                        .poolSizeCount = 2,
                                        .pPoolSizes = poolSizes,
                                        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT };
    VkResult r = vkCreateDescriptorPool(ctx->device, &dpci, NULL, &ctx->descriptorPool);
    if (r == VK_SUCCESS) { atomic_store(&ctx->desc_ring_head, 0); ctx->desc_ring_max = 512; }
    return r;
}

/* Staging pool init: single buffer, host-visible coherent memory */
static VkResult init_staging_pool(struct XenoBCContext *ctx, size_t pool_size)
{
    ctx->stagingSize = pool_size;
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                               .size = pool_size,
                               .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VkResult r = vkCreateBuffer(ctx->device, &bci, NULL, &ctx->stagingBuffer);
    if (r != VK_SUCCESS) return r;

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(ctx->device, ctx->stagingBuffer, &mr);
    uint32_t mem_idx = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties pr; vkGetPhysicalDeviceMemoryProperties(ctx->physical, &pr);
    for (uint32_t i=0;i<pr.memoryTypeCount;++i) {
        if ((mr.memoryTypeBits & (1u << i)) &&
            (pr.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { mem_idx = i; break; }
    }
    if (mem_idx == UINT32_MAX) { vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return VK_ERROR_MEMORY_MAP_FAILED; }

    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = mem_idx };
    r = vkAllocateMemory(ctx->device, &mai, NULL, &ctx->stagingMemory);
    if (r != VK_SUCCESS) { vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return r; }
    r = vkBindBufferMemory(ctx->device, ctx->stagingBuffer, ctx->stagingMemory, 0);
    if (r != VK_SUCCESS) { vkFreeMemory(ctx->device, ctx->stagingMemory, NULL); vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return r; }

    atomic_store(&ctx->staging_head, 0);
    return VK_SUCCESS;
}

/* Stage data into ring-style staging buffer. Fast, no fencing. Caller must ensure lifetime until submit. */
static VkResult stage_into_pool(struct XenoBCContext *ctx, const void *data, size_t size, VkDeviceSize *out_offset)
{
    if (!ctx || !data || size == 0 || !out_offset) return VK_ERROR_INITIALIZATION_FAILED;
    const size_t align = 64; /* alignment preferred for Xclipse */
    size_t alloc = (size + align - 1) & ~(align - 1);
    size_t head = (size_t)atomic_fetch_add(&ctx->staging_head, alloc);
    if (head + alloc > ctx->stagingSize) {
        /* wrap to 0 quickly — assumes older submissions have completed in this short-run usage model */
        atomic_store(&ctx->staging_head, 0);
        head = 0;
    }
    void *mapped = NULL;
    VkResult r = vkMapMemory(ctx->device, ctx->stagingMemory, head, alloc, 0, &mapped);
    if (r != VK_SUCCESS) return r;
    memcpy(mapped, data, size);
    vkUnmapMemory(ctx->device, ctx->stagingMemory);
    *out_offset = (VkDeviceSize)head;
    return VK_SUCCESS;
}

/* Create all modules and pipelines (called at init). Uses generated symbols declared in headers. */
VkResult xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical, VkQueue queue, struct XenoBCContext **out_ctx)
{
    if (!device || !physical || !queue || !out_ctx) return VK_ERROR_INITIALIZATION_FAILED;

    struct XenoBCContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ctx->device = device; ctx->physical = physical; ctx->queue = queue;

    vkGetPhysicalDeviceProperties(physical, &ctx->physProps);
    ctx->subgroup_size = 1; /* read features if you need subgroup ops */

    VkResult r = VK_SUCCESS;

    /* create layouts & pools */
    r = create_layouts(ctx); if (r != VK_SUCCESS) goto fail;
    r = create_descriptor_pool(ctx); if (r != VK_SUCCESS) goto fail;

    /* create modules - map arrays in order bc1..bc7 */
    const uint32_t *modules_words[7] = {
        bc1_shader_spv, bc2_shader_spv, bc3_shader_spv, bc4_shader_spv, bc5_shader_spv, bc6h_shader_spv, bc7_shader_spv
    };
    const size_t modules_size[7] = {
        bc1_shader_spv_len, bc2_shader_spv_len, bc3_shader_spv_len, bc4_shader_spv_len, bc5_shader_spv_len, bc6h_shader_spv_len, bc7_shader_spv_len
    };

    for (int i = 0; i < 7; ++i) {
        if (modules_size[i] > 0) {
            r = create_shader_module(device, modules_words[i], modules_size[i], &ctx->bcModules[i]);
            if (r != VK_SUCCESS) goto fail;
        } else {
            ctx->bcModules[i] = VK_NULL_HANDLE;
        }
    }

    /* create staging pool sized per macro */
    r = init_staging_pool(ctx, (size_t)EXYNOSTOOLS_STAGING_POOL_SIZE); if (r != VK_SUCCESS) goto fail;

    /* create per-module pipelines */
    for (int i = 0; i < 7; ++i) {
        if (ctx->bcModules[i]) {
            r = create_pipeline_for_module(ctx, ctx->bcModules[i], &ctx->bcPipelines[i]);
            if (r != VK_SUCCESS) goto fail;
        } else ctx->bcPipelines[i] = VK_NULL_HANDLE;
    }

    *out_ctx = ctx;
    return VK_SUCCESS;

fail:
    /* cleanup on failure */
    if (ctx) {
        for (int i = 0; i < 7; ++i) if (ctx->bcModules[i]) vkDestroyShaderModule(device, ctx->bcModules[i], NULL);
        if (ctx->pipelineLayout) vkDestroyPipelineLayout(device, ctx->pipelineLayout, NULL);
        if (ctx->descriptorSetLayout) vkDestroyDescriptorSetLayout(device, ctx->descriptorSetLayout, NULL);
        if (ctx->descriptorPool) vkDestroyDescriptorPool(device, ctx->descriptorPool, NULL);
        if (ctx->stagingMemory) vkFreeMemory(device, ctx->stagingMemory, NULL);
        if (ctx->stagingBuffer) vkDestroyBuffer(device, ctx->stagingBuffer, NULL);
        free(ctx);
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

/* Destroy context and all resources */
void xeno_bc_destroy_context(struct XenoBCContext *ctx)
{
    if (!ctx) return;
    VkDevice device = ctx->device;
    for (int i = 0; i < 7; ++i) if (ctx->bcPipelines[i]) vkDestroyPipeline(device, ctx->bcPipelines[i], NULL);
    for (int i = 0; i < 7; ++i) if (ctx->bcModules[i]) vkDestroyShaderModule(device, ctx->bcModules[i], NULL);
    if (ctx->pipelineLayout) vkDestroyPipelineLayout(device, ctx->pipelineLayout, NULL);
    if (ctx->descriptorSetLayout) vkDestroyDescriptorSetLayout(device, ctx->descriptorSetLayout, NULL);
    if (ctx->descriptorPool) vkDestroyDescriptorPool(device, ctx->descriptorPool, NULL);
    if (ctx->stagingMemory) vkFreeMemory(device, ctx->stagingMemory, NULL);
    if (ctx->stagingBuffer) vkDestroyBuffer(device, ctx->stagingBuffer, NULL);
    free(ctx);
}

/* Hot decode path — minimal overhead: stage data, update descriptor, bind, dispatch */
VkResult xeno_bc_decode_image(VkCommandBuffer cmd,
                              struct XenoBCContext *ctx,
                              const void *host_data,
                              size_t host_size,
                              VkBuffer src_buffer,
                              VkImageView dst_view,
                              VkImageBCFormat format,
                              VkExtent3D extent)
{
    if (!ctx || !cmd) return VK_ERROR_INITIALIZATION_FAILED;

    int idx = bc_format_index(format);
    if (idx < 0) { LOG_E("Unsupported BC format: %d", (int)format); return VK_ERROR_FORMAT_NOT_SUPPORTED; }
    VkPipeline pipeline = ctx->bcPipelines[idx];
    if (!pipeline) { LOG_E("Pipeline not available for BC format %d", (int)format); return VK_ERROR_INITIALIZATION_FAILED; }

    VkResult r = VK_SUCCESS;
    VkDescriptorSet descSet = VK_NULL_HANDLE;

    /* Allocate descriptor set from pool quickly */
    VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = ctx->descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &ctx->descriptorSetLayout };
    r = vkAllocateDescriptorSets(ctx->device, &dsai, &descSet);
    if (r != VK_SUCCESS) { LOG_E("Descriptor alloc failed: %d", r); return r; }

    if (host_data && host_size > 0) {
        VkDeviceSize offset = 0;
        r = stage_into_pool(ctx, host_data, host_size, &offset);
        if (r != VK_SUCCESS) { LOG_E("stage_into_pool failed: %d", r); return r; }
        VkDescriptorBufferInfo dbi = { ctx->stagingBuffer, offset, host_size };
        VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descSet, .dstBinding = BINDING_SRC_BUFFER, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi };
        vkUpdateDescriptorSets(ctx->device, 1, &w, 0, NULL);
    } else {
        VkDescriptorBufferInfo dbi = { src_buffer, 0, VK_WHOLE_SIZE };
        VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descSet, .dstBinding = BINDING_SRC_BUFFER, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi };
        vkUpdateDescriptorSets(ctx->device, 1, &w, 0, NULL);
    }

    if (dst_view != VK_NULL_HANDLE) {
        VkDescriptorImageInfo dii = { .imageView = dst_view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
        VkWriteDescriptorSet w = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descSet, .dstBinding = BINDING_DST_IMAGE, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &dii };
        vkUpdateDescriptorSets(ctx->device, 1, &w, 0, NULL);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->pipelineLayout, 0, 1, &descSet, 0, NULL);

    uint32_t push[4] = { 0, 0, extent.width, extent.height };
    vkCmdPushConstants(cmd, ctx->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);

    uint32_t gx = (extent.width + XCLIPSE_LOCAL_X - 1) / XCLIPSE_LOCAL_X;
    uint32_t gy = (extent.height + XCLIPSE_LOCAL_Y - 1) / XCLIPSE_LOCAL_Y;
    uint32_t gz = extent.depth ? extent.depth : 1;
    vkCmdDispatch(cmd, gx, gy, gz);

    /* Free descriptor set for reuse - fast path if pool supports freeing */
    vkFreeDescriptorSets(ctx->device, ctx->descriptorPool, 1, &descSet);

    return VK_SUCCESS;
}
