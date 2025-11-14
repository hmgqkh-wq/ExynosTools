// src/bc_emulate.c
// High-performance BC decode pipeline tuned for Xclipse 940. Requires generated bcN_shader.h headers.

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <vulkan/vulkan.h>

#include "xeno_bc.h"
#include "logging.h"
#include "xeno_log.h"

/* Require generated headers (no fallbacks) */
#include "bc1_shader.h"
#include "bc2_shader.h"
#include "bc3_shader.h"
#include "bc4_shader.h"
#include "bc5_shader.h"
#include "bc6h_shader.h"
#include "bc7_shader.h"

/* Local workgroup tuning for Xclipse 940 (specialization constants) */
#ifndef XCLIPSE_LOCAL_X
#define XCLIPSE_LOCAL_X 16u
#endif
#ifndef XCLIPSE_LOCAL_Y
#define XCLIPSE_LOCAL_Y 8u
#endif

#ifndef EXYNOSTOOLS_STAGING_POOL_SIZE
#define EXYNOSTOOLS_STAGING_POOL_SIZE (1 << 20) /* 1 MiB default; tune in CMake if needed */
#endif

#define BINDING_SRC_BUFFER 0
#define BINDING_DST_IMAGE  1

#define LOG_E(...) logging_error(__VA_ARGS__)
#define LOG_I(...) logging_info(__VA_ARGS__)

/* Context */
struct XenoBCContext {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;

    VkShaderModule bcModules[7];   /* bc1..bc7 */
    VkPipeline bcPipelines[7];

    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    size_t stagingSize;
    _Atomic size_t staging_head;

    VkPhysicalDeviceProperties physProps;
};

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

static VkResult create_shader_module_from_words(VkDevice device, const uint32_t *data, size_t size, VkShaderModule *out)
{
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    if (!data || size == 0) { *out = VK_NULL_HANDLE; return VK_SUCCESS; }
    VkShaderModuleCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = data };
    return vkCreateShaderModule(device, &ci, NULL, out);
}

static VkResult create_layouts_and_pipeline_layout(VkDevice device, VkDescriptorSetLayout *outDsl, VkPipelineLayout *outPl)
{
    VkDescriptorSetLayoutBinding bindings[2];
    bindings[0].binding = BINDING_SRC_BUFFER;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = NULL;

    bindings[1].binding = BINDING_DST_IMAGE;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo dslci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2, .pBindings = bindings };
    VkResult r = vkCreateDescriptorSetLayout(device, &dslci, NULL, outDsl);
    if (r != VK_SUCCESS) return r;

    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(uint32_t) * 4 };
    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = outDsl, .pushConstantRangeCount = 1, .pPushConstantRanges = &pcr };
    return vkCreatePipelineLayout(device, &plci, NULL, outPl);
}

static VkResult create_pipeline_for_module(VkDevice device, VkPipelineLayout layout, VkShaderModule module, VkPipeline *out)
{
    if (!module) { *out = VK_NULL_HANDLE; return VK_SUCCESS; }

    VkSpecializationMapEntry map[2];
    map[0].constantID = 0; map[0].offset = 0; map[0].size = sizeof(uint32_t);
    map[1].constantID = 1; map[1].offset = sizeof(uint32_t); map[1].size = sizeof(uint32_t);
    uint32_t specData[2] = { XCLIPSE_LOCAL_X, XCLIPSE_LOCAL_Y };

    VkSpecializationInfo spec = { .mapEntryCount = 2, .pMapEntries = map, .dataSize = sizeof(specData), .pData = specData };

    VkPipelineShaderStageCreateInfo stage = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main", .pSpecializationInfo = &spec };

    VkComputePipelineCreateInfo cpci = { .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = stage, .layout = layout };
    return vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, NULL, out);
}

static VkResult create_descriptor_pool(VkDevice device, VkDescriptorPool *outPool)
{
    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; poolSizes[0].descriptorCount = 1024;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;  poolSizes[1].descriptorCount = 1024;

    VkDescriptorPoolCreateInfo dpci = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1024, .poolSizeCount = 2, .pPoolSizes = poolSizes, .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT };
    return vkCreateDescriptorPool(device, &dpci, NULL, outPool);
}

static VkResult init_staging_pool(VkDevice device, VkPhysicalDevice physical, VkBuffer *outBuf, VkDeviceMemory *outMem, size_t pool_size)
{
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = pool_size, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VkResult r = vkCreateBuffer(device, &bci, NULL, outBuf);
    if (r != VK_SUCCESS) return r;

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(device, *outBuf, &mr);

    VkPhysicalDeviceMemoryProperties pr; vkGetPhysicalDeviceMemoryProperties(physical, &pr);
    uint32_t mem_idx = UINT32_MAX;
    for (uint32_t i = 0; i < pr.memoryTypeCount; ++i) {
        if ((mr.memoryTypeBits & (1u << i)) && (pr.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { mem_idx = i; break; }
    }
    if (mem_idx == UINT32_MAX) { vkDestroyBuffer(device, *outBuf, NULL); return VK_ERROR_MEMORY_MAP_FAILED; }

    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = mem_idx };
    r = vkAllocateMemory(device, &mai, NULL, outMem);
    if (r != VK_SUCCESS) { vkDestroyBuffer(device, *outBuf, NULL); return r; }
    r = vkBindBufferMemory(device, *outBuf, *outMem, 0);
    if (r != VK_SUCCESS) { vkFreeMemory(device, *outMem, NULL); vkDestroyBuffer(device, *outBuf, NULL); return r; }
    return VK_SUCCESS;
}

VkResult xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical, VkQueue queue, struct XenoBCContext **out_ctx)
{
    if (!device || !physical || !queue || !out_ctx) return VK_ERROR_INITIALIZATION_FAILED;
    struct XenoBCContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ctx->device = device; ctx->physical = physical; ctx->queue = queue;

    vkGetPhysicalDeviceProperties(physical, &ctx->physProps);

    VkResult r = create_layouts_and_pipeline_layout(device, &ctx->descriptorSetLayout, &ctx->pipelineLayout);
    if (r != VK_SUCCESS) goto fail;

    r = create_descriptor_pool(device, &ctx->descriptorPool);
    if (r != VK_SUCCESS) goto fail;

    /* Create shader modules from generated symbols (bc1..bc7). Each header must provide bcN_shader_spv and bcN_shader_spv_len */
    const uint32_t *words[7] = { bc1_shader_spv, bc2_shader_spv, bc3_shader_spv, bc4_shader_spv, bc5_shader_spv, bc6h_shader_spv, bc7_shader_spv };
    const size_t sizes[7]     = { bc1_shader_spv_len, bc2_shader_spv_len, bc3_shader_spv_len, bc4_shader_spv_len, bc5_shader_spv_len, bc6h_shader_spv_len, bc7_shader_spv_len };

    for (int i = 0; i < 7; ++i) {
        if (sizes[i] > 0) {
            r = create_shader_module_from_words(device, words[i], sizes[i], &ctx->bcModules[i]);
            if (r != VK_SUCCESS) goto fail;
        } else ctx->bcModules[i] = VK_NULL_HANDLE;
    }

    r = init_staging_pool(device, physical, &ctx->stagingBuffer, &ctx->stagingMemory, (size_t)EXYNOSTOOLS_STAGING_POOL_SIZE);
    if (r != VK_SUCCESS) goto fail;
    atomic_store(&ctx->staging_head, 0);

    for (int i = 0; i < 7; ++i) {
        if (ctx->bcModules[i]) {
            r = create_pipeline_for_module(ctx->device, ctx->pipelineLayout, ctx->bcModules[i], &ctx->bcPipelines[i]);
            if (r != VK_SUCCESS) goto fail;
        } else ctx->bcPipelines[i] = VK_NULL_HANDLE;
    }

    *out_ctx = ctx;
    return VK_SUCCESS;

fail:
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

VkResult xeno_bc_decode_image(VkCommandBuffer cmd, struct XenoBCContext *ctx, const void *host_data, size_t host_size, VkBuffer src_buffer, VkImageView dst_view, VkImageBCFormat format, VkExtent3D extent)
{
    if (!ctx || !cmd) return VK_ERROR_INITIALIZATION_FAILED;
    int idx = bc_format_index(format);
    if (idx < 0) { LOG_E("Unsupported BC format %d", (int)format); return VK_ERROR_FORMAT_NOT_SUPPORTED; }
    VkPipeline pipeline = ctx->bcPipelines[idx];
    if (!pipeline) { LOG_E("Pipeline not available for BC format %d", (int)format); return VK_ERROR_INITIALIZATION_FAILED; }

    VkResult r;
    VkDescriptorSet descSet;
    VkDescriptorSetAllocateInfo dsai = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = ctx->descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &ctx->descriptorSetLayout };
    r = vkAllocateDescriptorSets(ctx->device, &dsai, &descSet);
    if (r != VK_SUCCESS) { LOG_E("Descriptor alloc failed: %d", r); return r; }

    if (host_data && host_size > 0) {
        VkDeviceSize offset = 0;
        /* stage into ring-staging buffer (caller must ensure data lifetime until submit) */
        const size_t align = 64;
        size_t alloc = (host_size + align - 1) & ~(align - 1);
        size_t head = (size_t)atomic_fetch_add(&ctx->staging_head, alloc);
        if (head + alloc > ctx->stagingSize) { atomic_store(&ctx->staging_head, 0); head = 0; }
        void *mapped = NULL; r = vkMapMemory(ctx->device, ctx->stagingMemory, head, alloc, 0, &mapped); if (r != VK_SUCCESS) return r;
        memcpy(mapped, host_data, host_size); vkUnmapMemory(ctx->device, ctx->stagingMemory);
        offset = (VkDeviceSize)head;
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

    vkFreeDescriptorSets(ctx->device, ctx->descriptorPool, 1, &descSet);
    return VK_SUCCESS;
}
