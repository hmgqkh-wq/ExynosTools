// src/bc_emulate.c
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <vulkan/vulkan.h>

#include "xeno_bc.h"
#include "logging.h"
#include "xeno_log.h"
#include "bc_shaders_fallback.h"

#if defined(__has_include)
  #if __has_include("bc1_shader.h")
    #include "bc1_shader.h"
    #define HAVE_BC1 1
  #endif
  #if __has_include("bc2_shader.h")
    #include "bc2_shader.h"
    #define HAVE_BC2 1
  #endif
  #if __has_include("bc3_shader.h")
    #include "bc3_shader.h"
    #define HAVE_BC3 1
  #endif
  #if __has_include("bc4_shader.h")
    #include "bc4_shader.h"
    #define HAVE_BC4 1
  #endif
  #if __has_include("bc5_shader.h")
    #include "bc5_shader.h"
    #define HAVE_BC5 1
  #endif
  #if __has_include("bc6h_shader.h")
    #include "bc6h_shader.h"
    #define HAVE_BC6H 1
  #endif
  #if __has_include("bc7_shader.h")
    #include "bc7_shader.h"
    #define HAVE_BC7 1
  #endif
#endif

#ifndef XCLIPSE_DEFAULT_LOCAL_X
#define XCLIPSE_DEFAULT_LOCAL_X 16u
#endif
#ifndef XCLIPSE_DEFAULT_LOCAL_Y
#define XCLIPSE_DEFAULT_LOCAL_Y 8u
#endif
#ifndef EXYNOSTOOLS_STAGING_POOL_SIZE
#define EXYNOSTOOLS_STAGING_POOL_SIZE (1 << 20)
#endif

#define BINDING_SRC_BUFFER 0
#define BINDING_DST_IMAGE  1

#define LOG_E(...) logging_error(__VA_ARGS__)
#define LOG_I(...) logging_info(__VA_ARGS__)

struct XenoBCContext {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;
    uint32_t queue_family_index;

    VkShaderModule bc1Module;
    VkShaderModule bc2Module;
    VkShaderModule bc3Module;
    VkShaderModule bc4Module;
    VkShaderModule bc5Module;
    VkShaderModule bc6hModule;
    VkShaderModule bc7Module;

    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;

    VkPipeline bc1Pipeline;
    VkPipeline bc2Pipeline;
    VkPipeline bc3Pipeline;
    VkPipeline bc4Pipeline;
    VkPipeline bc5Pipeline;
    VkPipeline bc6hPipeline;
    VkPipeline bc7Pipeline;

    VkDescriptorPool descriptorPool;
    atomic_uint desc_ring_head;
    uint32_t desc_ring_max;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    size_t stagingSize;
    atomic_size_t staging_head;

    VkPhysicalDeviceProperties physProps;
    uint32_t subgroup_size;
};

static VkResult create_shader_module_from_bytes(VkDevice device, const uint32_t *data, size_t size, VkShaderModule *out)
{
    if (!device || !out) return VK_ERROR_INITIALIZATION_FAILED;
    if (!data || size == 0) { *out = VK_NULL_HANDLE; return VK_SUCCESS; }

    VkShaderModuleCreateInfo ci = {0};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode = data;
    return vkCreateShaderModule(device, &ci, NULL, out);
}

static uint32_t find_memory_type_index(VkPhysicalDevice physical, uint32_t typeBits, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(physical, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return UINT32_MAX;
}

static VkResult create_compute_pipeline_with_spec(struct XenoBCContext *ctx, VkShaderModule module, VkPipeline *out)
{
    if (!ctx || !out) return VK_ERROR_INITIALIZATION_FAILED;
    if (!module) { *out = VK_NULL_HANDLE; return VK_SUCCESS; }

    VkSpecializationMapEntry map[2];
    map[0].constantID = 0; map[0].offset = 0; map[0].size = sizeof(uint32_t);
    map[1].constantID = 1; map[1].offset = sizeof(uint32_t); map[1].size = sizeof(uint32_t);
    uint32_t specData[2] = { XCLIPSE_DEFAULT_LOCAL_X, XCLIPSE_DEFAULT_LOCAL_Y };

    VkSpecializationInfo spec = {0};
    spec.mapEntryCount = 2;
    spec.pMapEntries = map;
    spec.dataSize = sizeof(specData);
    spec.pData = specData;

    VkPipelineShaderStageCreateInfo stage = {0};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName = "main";
    stage.pSpecializationInfo = &spec;

    VkComputePipelineCreateInfo cpci = {0};
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage = stage;
    cpci.layout = ctx->pipelineLayout;

    return vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, &cpci, NULL, out);
}

static VkResult create_common_layouts_and_pipelines(struct XenoBCContext *ctx)
{
    if (!ctx) return VK_ERROR_INITIALIZATION_FAILED;
    VkResult r;

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

    VkDescriptorSetLayoutCreateInfo dslci = {0};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;

    r = vkCreateDescriptorSetLayout(ctx->device, &dslci, NULL, &ctx->descriptorSetLayout);
    if (r != VK_SUCCESS) { LOG_E("vkCreateDescriptorSetLayout failed: %d", r); return r; }

    VkPipelineLayoutCreateInfo plci = {0};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &ctx->descriptorSetLayout;

    VkPushConstantRange pcr = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) * 4 };
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;

    r = vkCreatePipelineLayout(ctx->device, &plci, NULL, &ctx->pipelineLayout);
    if (r != VK_SUCCESS) { LOG_E("vkCreatePipelineLayout failed: %d", r); return r; }

    r = create_compute_pipeline_with_spec(ctx, ctx->bc1Module, &ctx->bc1Pipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc1 failed: %d", r); return r; }
    r = create_compute_pipeline_with_spec(ctx, ctx->bc2Module, &ctx->bc2Pipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc2 failed: %d", r); return r; }
    r = create_compute_pipeline_with_spec(ctx, ctx->bc3Module, &ctx->bc3Pipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc3 failed: %d", r); return r; }
    r = create_compute_pipeline_with_spec(ctx, ctx->bc4Module, &ctx->bc4Pipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc4 failed: %d", r); return r; }
    r = create_compute_pipeline_with_spec(ctx, ctx->bc5Module, &ctx->bc5Pipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc5 failed: %d", r); return r; }
    r = create_compute_pipeline_with_spec(ctx, ctx->bc6hModule, &ctx->bc6hPipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc6h failed: %d", r); return r; }
    r = create_compute_pipeline_with_spec(ctx, ctx->bc7Module, &ctx->bc7Pipeline); if (r != VK_SUCCESS) { LOG_E("create pipeline bc7 failed: %d", r); return r; }

    return VK_SUCCESS;
}

static void destroy_common_layouts_and_pipelines(struct XenoBCContext *ctx)
{
    if (!ctx) return;
    if (ctx->bc1Pipeline) vkDestroyPipeline(ctx->device, ctx->bc1Pipeline, NULL);
    if (ctx->bc2Pipeline) vkDestroyPipeline(ctx->device, ctx->bc2Pipeline, NULL);
    if (ctx->bc3Pipeline) vkDestroyPipeline(ctx->device, ctx->bc3Pipeline, NULL);
    if (ctx->bc4Pipeline) vkDestroyPipeline(ctx->device, ctx->bc4Pipeline, NULL);
    if (ctx->bc5Pipeline) vkDestroyPipeline(ctx->device, ctx->bc5Pipeline, NULL);
    if (ctx->bc6hPipeline) vkDestroyPipeline(ctx->device, ctx->bc6hPipeline, NULL);
    if (ctx->bc7Pipeline) vkDestroyPipeline(ctx->device, ctx->bc7Pipeline, NULL);

    if (ctx->pipelineLayout) vkDestroyPipelineLayout(ctx->device, ctx->pipelineLayout, NULL);
    if (ctx->descriptorSetLayout) vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, NULL);
}

static VkResult ensure_descriptor_pool(struct XenoBCContext *ctx)
{
    if (!ctx) return VK_ERROR_INITIALIZATION_FAILED;
    if (ctx->descriptorPool) return VK_SUCCESS;

    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; poolSizes[0].descriptorCount = 256;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;  poolSizes[1].descriptorCount = 256;

    VkDescriptorPoolCreateInfo dpci = {0};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 256;
    dpci.poolSizeCount = 2;
    dpci.pPoolSizes = poolSizes;
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkResult r = vkCreateDescriptorPool(ctx->device, &dpci, NULL, &ctx->descriptorPool);
    if (r == VK_SUCCESS) { atomic_store(&ctx->desc_ring_head, 0); ctx->desc_ring_max = 256; }
    return r;
}

static VkResult init_staging_pool(struct XenoBCContext *ctx, size_t pool_size)
{
    if (!ctx || pool_size == 0) return VK_ERROR_INITIALIZATION_FAILED;
    ctx->stagingSize = pool_size;

    VkBufferCreateInfo bci = {0};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = pool_size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult r = vkCreateBuffer(ctx->device, &bci, NULL, &ctx->stagingBuffer);
    if (r != VK_SUCCESS) return r;

    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(ctx->device, ctx->stagingBuffer, &mr);
    if (ctx->physical == VK_NULL_HANDLE) { LOG_E("physical device not set"); vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return VK_ERROR_INITIALIZATION_FAILED; }

    uint32_t memIndex = find_memory_type_index(ctx->physical, mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memIndex == UINT32_MAX) { vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return VK_ERROR_MEMORY_MAP_FAILED; }

    VkMemoryAllocateInfo mai = {0}; mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; mai.allocationSize = mr.size; mai.memoryTypeIndex = memIndex;
    r = vkAllocateMemory(ctx->device, &mai, NULL, &ctx->stagingMemory); if (r != VK_SUCCESS) { vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return r; }
    r = vkBindBufferMemory(ctx->device, ctx->stagingBuffer, ctx->stagingMemory, 0); if (r != VK_SUCCESS) { vkFreeMemory(ctx->device, ctx->stagingMemory, NULL); vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); return r; }

    atomic_store(&ctx->staging_head, 0);
    return VK_SUCCESS;
}

static void destroy_staging_pool(struct XenoBCContext *ctx)
{
    if (!ctx) return;
    if (ctx->stagingMemory) { vkFreeMemory(ctx->device, ctx->stagingMemory, NULL); ctx->stagingMemory = VK_NULL_HANDLE; }
    if (ctx->stagingBuffer) { vkDestroyBuffer(ctx->device, ctx->stagingBuffer, NULL); ctx->stagingBuffer = VK_NULL_HANDLE; }
}

static VkResult stage_into_pool(struct XenoBCContext *ctx, const void *data, size_t size, VkDeviceSize *out_offset)
{
    if (!ctx || !data || size == 0 || !out_offset) return VK_ERROR_INITIALIZATION_FAILED;
    const size_t align = 64u;
    size_t alloc = (size + align - 1) & ~(align - 1);
    size_t head = (size_t)atomic_fetch_add(&ctx->staging_head, alloc);
    if (head + alloc > ctx->stagingSize) { atomic_store(&ctx->staging_head, 0); head = 0; }

    void *mapped = NULL; VkResult r = vkMapMemory(ctx->device, ctx->stagingMemory, head, alloc, 0, &mapped);
    if (r != VK_SUCCESS) return r;
    memcpy(mapped, data, size);
    vkUnmapMemory(ctx->device, ctx->stagingMemory);

    *out_offset = (VkDeviceSize)head;
    return VK_SUCCESS;
}

VkResult xeno_bc_create_context(VkDevice device, struct XenoBCContext **out_ctx)
{
    if (!device || !out_ctx) return VK_ERROR_INITIALIZATION_FAILED;
    struct XenoBCContext *ctx = (struct XenoBCContext*)calloc(1, sizeof(*ctx));
    if (!ctx) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ctx->device = device;

    ctx->physProps = (VkPhysicalDeviceProperties){0};

    VkResult r = VK_SUCCESS;

    if ((sizeof(bc1_shader_spv) > 0) && (bc1_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc1_shader_spv, bc1_shader_spv_len, &ctx->bc1Module); if (r != VK_SUCCESS) goto fail; }
    if ((sizeof(bc2_shader_spv) > 0) && (bc2_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc2_shader_spv, bc2_shader_spv_len, &ctx->bc2Module); if (r != VK_SUCCESS) goto fail; }
    if ((sizeof(bc3_shader_spv) > 0) && (bc3_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc3_shader_spv, bc3_shader_spv_len, &ctx->bc3Module); if (r != VK_SUCCESS) goto fail; }
    if ((sizeof(bc4_shader_spv) > 0) && (bc4_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc4_shader_spv, bc4_shader_spv_len, &ctx->bc4Module); if (r != VK_SUCCESS) goto fail; }
    if ((sizeof(bc5_shader_spv) > 0) && (bc5_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc5_shader_spv, bc5_shader_spv_len, &ctx->bc5Module); if (r != VK_SUCCESS) goto fail; }
    if ((sizeof(bc6h_shader_spv) > 0) && (bc6h_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc6h_shader_spv, bc6h_shader_spv_len, &ctx->bc6hModule); if (r != VK_SUCCESS) goto fail; }
    if ((sizeof(bc7_shader_spv) > 0) && (bc7_shader_spv_len > 0)) { r = create_shader_module_from_bytes(device, bc7_shader_spv, bc7_shader_spv_len, &ctx->bc7Module); if (r != VK_SUCCESS) goto fail; }

    r = create_common_layouts_and_pipelines(ctx); if (r != VK_SUCCESS) { LOG_E("create_common_layouts_and_pipelines failed: %d", r); goto fail; }
    r = init_staging_pool(ctx, (size_t)EXYNOSTOOLS_STAGING_POOL_SIZE); if (r != VK_SUCCESS) { LOG_E("init_staging_pool failed: %d", r); goto fail; }
    r = ensure_descriptor_pool(ctx); if (r != VK_SUCCESS) { LOG_E("ensure_descriptor_pool failed: %d", r); goto fail; }

    *out_ctx = ctx; return VK_SUCCESS;

fail:
    if (ctx) {
        destroy_common_layouts_and_pipelines(ctx);
        destroy_staging_pool(ctx);
        if (ctx->bc1Module) vkDestroyShaderModule(device, ctx->bc1Module, NULL);
        if (ctx->bc2Module) vkDestroyShaderModule(device, ctx->bc2Module, NULL);
        if (ctx->bc3Module) vkDestroyShaderModule(device, ctx->bc3Module, NULL);
        if (ctx->bc4Module) vkDestroyShaderModule(device, ctx->bc4Module, NULL);
        if (ctx->bc5Module) vkDestroyShaderModule(device, ctx->bc5Module, NULL);
        if (ctx->bc6hModule) vkDestroyShaderModule(device, ctx->bc6hModule, NULL);
        if (ctx->bc7Module) vkDestroyShaderModule(device, ctx->bc7Module, NULL);
        free(ctx);
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}

void xeno_bc_destroy_context(struct XenoBCContext *ctx)
{
    if (!ctx) return;
    if (ctx->device) {
        destroy_common_layouts_and_pipelines(ctx);
        destroy_staging_pool(ctx);
        if (ctx->bc1Module) vkDestroyShaderModule(ctx->device, ctx->bc1Module, NULL);
        if (ctx->bc2Module) vkDestroyShaderModule(ctx->device, ctx->bc2Module, NULL);
        if (ctx->bc3Module) vkDestroyShaderModule(ctx->device, ctx->bc3Module, NULL);
        if (ctx->bc4Module) vkDestroyShaderModule(ctx->device, ctx->bc4Module, NULL);
        if (ctx->bc5Module) vkDestroyShaderModule(ctx->device, ctx->bc5Module, NULL);
        if (ctx->bc6hModule) vkDestroyShaderModule(ctx->device, ctx->bc6hModule, NULL);
        if (ctx->bc7Module) vkDestroyShaderModule(ctx->device, ctx->bc7Module, NULL);
        if (ctx->descriptorPool) vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, NULL);
    }
    free(ctx);
}

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

    VkPipeline pipeline = VK_NULL_HANDLE;
    switch (format) {
        case VK_IMAGE_BC1: pipeline = ctx->bc1Pipeline; break;
        case VK_IMAGE_BC3: pipeline = ctx->bc3Pipeline; break;
        case VK_IMAGE_BC4: pipeline = ctx->bc4Pipeline; break;
        case VK_IMAGE_BC5: pipeline = ctx->bc5Pipeline; break;
        case VK_IMAGE_BC6H: pipeline = ctx->bc6hPipeline; break;
        case VK_IMAGE_BC7: pipeline = ctx->bc7Pipeline; break;
        default: LOG_E("Unsupported BC format: %d", (int)format); return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    if (!pipeline) { LOG_E("Pipeline for format not created"); return VK_ERROR_INITIALIZATION_FAILED; }

    VkResult r = ensure_descriptor_pool(ctx);
    if (r != VK_SUCCESS) return r;

    VkDescriptorSet descSet = VK_NULL_HANDLE;
    r = allocate_descriptor_from_ring(ctx, ctx->descriptorSetLayout, &descSet);
    if (r != VK_SUCCESS) { LOG_E("allocate_descriptor_from_ring failed: %d", r); return r; }

    if (host_data && host_size > 0) {
        VkDeviceSize offset = 0;
        r = stage_into_pool(ctx, host_data, host_size, &offset);
        if (r != VK_SUCCESS) { LOG_E("stage_into_pool failed: %d", r); return r; }
        VkDescriptorBufferInfo dbi = { ctx->stagingBuffer, offset, host_size };
        VkWriteDescriptorSet w = {0};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSet;
        w.dstBinding = BINDING_SRC_BUFFER;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(ctx->device, 1, &w, 0, NULL);
    } else {
        if (src_buffer == VK_NULL_HANDLE) { LOG_E("No source provided"); return VK_ERROR_INITIALIZATION_FAILED; }
        VkDescriptorBufferInfo dbi = { src_buffer, 0, VK_WHOLE_SIZE };
        VkWriteDescriptorSet w = {0};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSet;
        w.dstBinding = BINDING_SRC_BUFFER;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(ctx->device, 1, &w, 0, NULL);
    }

    if (dst_view != VK_NULL_HANDLE) {
        VkDescriptorImageInfo dii = {0};
        dii.imageView = dst_view;
        dii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w = {0};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSet;
        w.dstBinding = BINDING_DST_IMAGE;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w.pImageInfo = &dii;
        vkUpdateDescriptorSets(ctx->device, 1, &w, 0, NULL);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->pipelineLayout, 0, 1, &descSet, 0, NULL);

    uint32_t push[4] = { 0, 0, extent.width, extent.height };
    vkCmdPushConstants(cmd, ctx->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);

    uint32_t gx = (extent.width + XCLIPSE_DEFAULT_LOCAL_X - 1) / XCLIPSE_DEFAULT_LOCAL_X;
    uint32_t gy = (extent.height + XCLIPSE_DEFAULT_LOCAL_Y - 1) / XCLIPSE_DEFAULT_LOCAL_Y;
    uint32_t gz = extent.depth ? extent.depth : 1;
    vkCmdDispatch(cmd, gx, gy, gz);

    return VK_SUCCESS;
}
