/*
  src/bc_emulate.c
  High-performance, non-fallback BC decode pipeline implementation
  tuned for Samsung Xclipse 940.

  Requirements:
  - Generated SPIR-V headers must exist in build include path:
      bc1_shader.h, bc2_shader.h, bc3_shader.h, bc4_shader.h, bc5_shader.h, bc6h_shader.h, bc7_shader.h
    Each header must expose:
      const uint32_t <name>_spv[] = {...};
      const size_t   <name>_spv_len = sizeof(<name>_spv);
  - Vulkan device with compute queue.
  - No fallback code paths; if a pipeline/shader is missing it reports error.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

#include <vulkan/vulkan.h>

#include "xeno_bc.h"
#include "logging.h"    /* logging_error / logging_info; provide in repo */
#include "xeno_log.h"   /* xeno_log_stream() */

#define XCLIPSE_LOCAL_X 16u
#define XCLIPSE_LOCAL_Y 8u
#define STAGING_POOL_DEFAULT (1 << 20) /* 1MB default; adjustable via compile define */

#define BINDING_SRC_BUFFER 0
#define BINDING_DST_IMAGE  1

struct XenoBCContext {
    VkDevice device;
    VkPhysicalDevice physical;
    VkQueue queue;

    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;

    VkShaderModule modules[7];
    VkPipeline pipelines[7];

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    size_t stagingSize;
    _Atomic size_t staging_head;

    VkPhysicalDeviceProperties physProps;
};

/* Forward declarations */
static VkResult create_descriptor_layouts(VkDevice dev, VkDescriptorSetLayout *outDsl, VkPipelineLayout *outPl);
static VkResult create_descriptor_pool(VkDevice dev, VkDescriptorPool *outPool);
static VkResult create_shader_module(VkDevice dev, const uint32_t *words, size_t size, VkShaderModule *outModule);
static VkResult create_compute_pipeline(VkDevice dev, VkPipelineLayout layout, VkShaderModule module, VkPipeline *outPipeline);
static VkResult init_staging_pool(VkDevice device, VkPhysicalDevice physical, VkBuffer *outBuf, VkDeviceMemory *outMem, size_t pool_size);

/* External: get local size */
void xeno_bc_get_optimal_local_size(uint32_t *local_x, uint32_t *local_y)
{
    if (local_x) *local_x = XCLIPSE_LOCAL_X;
    if (local_y) *local_y = XCLIPSE_LOCAL_Y;
}

/* Provide concise mapping from format enum to index */
static inline int bc_format_index(VkImageBCFormat f)
{
    switch (f) {
    case VK_IMAGE_BC1:  return 0;
    case VK_IMAGE_BC2:  return 1;
    case VK_IMAGE_BC3:  return 2;
    case VK_IMAGE_BC4:  return 3;
    case VK_IMAGE_BC5:  return 4;
    case VK_IMAGE_BC6H: return 5;
    case VK_IMAGE_BC7:  return 6;
    default: return -1;
    }
}

/* Create context: compile-time expects generated SPIR-V headers exist in include path.
   The pipeline creation here treats missing modules/pipelines as hard errors (non-fallback). */
VkResult xeno_bc_create_context(VkDevice device, VkPhysicalDevice physical, VkQueue queue, struct XenoBCContext **out_ctx)
{
    if (!device || !physical || !queue || !out_ctx) return VK_ERROR_INITIALIZATION_FAILED;

    struct XenoBCContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return VK_ERROR_OUT_OF_HOST_MEMORY;

    ctx->device = device;
    ctx->physical = physical;
    ctx->queue = queue;
    ctx->stagingSize = (size_t)STAGING_POOL_DEFAULT;
    atomic_store(&ctx->staging_head, 0);

    vkGetPhysicalDeviceProperties(physical, &ctx->physProps);

    VkResult r = create_descriptor_layouts(device, &ctx->descriptorSetLayout, &ctx->pipelineLayout);
    if (r != VK_SUCCESS) goto fail;

    r = create_descriptor_pool(device, &ctx->descriptorPool);
    if (r != VK_SUCCESS) goto fail;

    /* Load generated SPV headers - these must be present. */
    /* Headers must define: bcN_shader_spv and bcN_shader_spv_len */
    extern const uint32_t bc1_shader_spv[]; extern const size_t bc1_shader_spv_len;
    extern const uint32_t bc2_shader_spv[]; extern const size_t bc2_shader_spv_len;
    extern const uint32_t bc3_shader_spv[]; extern const size_t bc3_shader_spv_len;
    extern const uint32_t bc4_shader_spv[]; extern const size_t bc4_shader_spv_len;
    extern const uint32_t bc5_shader_spv[]; extern const size_t bc5_shader_spv_len;
    extern const uint32_t bc6h_shader_spv[]; extern const size_t bc6h_shader_spv_len;
    extern const uint32_t bc7_shader_spv[]; extern const size_t bc7_shader_spv_len;

    const uint32_t *words[7] = {
        bc1_shader_spv,
        bc2_shader_spv,
        bc3_shader_spv,
        bc4_shader_spv,
        bc5_shader_spv,
        bc6h_shader_spv,
        bc7_shader_spv
    };
    const size_t sizes[7] = {
        bc1_shader_spv_len,
        bc2_shader_spv_len,
        bc3_shader_spv_len,
        bc4_shader_spv_len,
        bc5_shader_spv_len,
        bc6h_shader_spv_len,
        bc7_shader_spv_len
    };

    for (int i = 0; i < 7; ++i) {
        if (sizes[i] == 0 || words[i] == NULL) {
            logging_error("Missing SPV for bc index %d; non-fallback policy enforces failure", i);
            r = VK_ERROR_INITIALIZATION_FAILED;
            goto fail;
        }
        r = create_shader_module(device, words[i], sizes[i], &ctx->modules[i]);
        if (r != VK_SUCCESS) { logging_error("vkCreateShaderModule failed for bc %d: %d", i, (int)r); goto fail; }
        r = create_compute_pipeline(device, ctx->pipelineLayout, ctx->modules[i], &ctx->pipelines[i]);
        if (r != VK_SUCCESS) { logging_error("vkCreateComputePipelines failed for bc %d: %d", i, (int)r); goto fail; }
    }

    r = init_staging_pool(device, physical, &ctx->stagingBuffer, &ctx->stagingMemory, ctx->stagingSize);
    if (r != VK_SUCCESS) { logging_error("init_staging_pool failed: %d", (int)r); goto fail; }

    *out_ctx = ctx;
    logging_info("xeno_bc_create_context: success (Xclipse 940 optimized)");
    return VK_SUCCESS;

fail:
    if (ctx) {
        for (int i = 0; i < 7; ++i) if (ctx->pipelines[i]) vkDestroyPipeline(device, ctx->pipelines[i], NULL);
        for (int i = 0; i < 7; ++i) if (ctx->modules[i]) vkDestroyShaderModule(device, ctx->modules[i], NULL);
        if (ctx->descriptorPool) vkDestroyDescriptorPool(device, ctx->descriptorPool, NULL);
        if (ctx->pipelineLayout) vkDestroyPipelineLayout(device, ctx->pipelineLayout, NULL);
        if (ctx->descriptorSetLayout) vkDestroyDescriptorSetLayout(device, ctx->descriptorSetLayout, NULL);
        if (ctx->stagingMemory) vkFreeMemory(device, ctx->stagingMemory, NULL);
        if (ctx->stagingBuffer) vkDestroyBuffer(device, ctx->stagingBuffer, NULL);
        free(ctx);
    }
    return r;
}

void xeno_bc_destroy_context(struct XenoBCContext *ctx)
{
    if (!ctx) return;
    VkDevice dev = ctx->device;
    for (int i = 0; i < 7; ++i) {
        if (ctx->pipelines[i]) vkDestroyPipeline(dev, ctx->pipelines[i], NULL);
        if (ctx->modules[i]) vkDestroyShaderModule(dev, ctx->modules[i], NULL);
    }
    if (ctx->descriptorPool) vkDestroyDescriptorPool(dev, ctx->descriptorPool, NULL);
    if (ctx->pipelineLayout) vkDestroyPipelineLayout(dev, ctx->pipelineLayout, NULL);
    if (ctx->descriptorSetLayout) vkDestroyDescriptorSetLayout(dev, ctx->descriptorSetLayout, NULL);
    if (ctx->stagingMemory) vkFreeMemory(dev, ctx->stagingMemory, NULL);
    if (ctx->stagingBuffer) vkDestroyBuffer(dev, ctx->stagingBuffer, NULL);
    free(ctx);
    logging_info("xeno_bc_destroy_context: cleaned up");
}

/* Record a decode dispatch into provided command buffer. Non-fallback: uses pipeline for format index. */
VkResult xeno_bc_decode_image(VkCommandBuffer cmd, struct XenoBCContext *ctx, const void *host_data, size_t host_size, VkBuffer src_buffer, VkImageView dst_view, VkImageBCFormat format, VkExtent3D extent)
{
    if (!cmd || !ctx) return VK_ERROR_INITIALIZATION_FAILED;

    int idx = bc_format_index(format);
    if (idx < 0) { logging_error("Unsupported BC format %d", (int)format); return VK_ERROR_FORMAT_NOT_SUPPORTED; }

    VkPipeline pipeline = ctx->pipelines[idx];
    if (!pipeline) { logging_error("Pipeline missing for format idx %d", idx); return VK_ERROR_INITIALIZATION_FAILED; }

    VkResult r;

    /* Allocate descriptor set */
    VkDescriptorSetLayout dsl = ctx->descriptorSetLayout;
    VkDescriptorSet descSet;
    VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx->descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &dsl
    };
    r = vkAllocateDescriptorSets(ctx->device, &dsai, &descSet);
    if (r != VK_SUCCESS) { logging_error("vkAllocateDescriptorSets failed: %d", (int)r); return r; }

    /* If host_data provided, stage into ring buffer */
    VkDescriptorBufferInfo dbi = { .buffer = VK_NULL_HANDLE, .offset = 0, .range = VK_WHOLE_SIZE };
    if (host_data && host_size > 0) {
        size_t align = 64;
        size_t alloc = (host_size + align - 1) & ~(align - 1);
        size_t head = (size_t)atomic_fetch_add(&ctx->staging_head, alloc);
        if (head + alloc > ctx->stagingSize) {
            /* wrap ring */
            atomic_store(&ctx->staging_head, 0);
            head = 0;
        }
        void *mapped = NULL;
        r = vkMapMemory(ctx->device, ctx->stagingMemory, head, alloc, 0, &mapped);
        if (r != VK_SUCCESS) { logging_error("vkMapMemory failed: %d", (int)r); vkFreeDescriptorSets(ctx->device, ctx->descriptorPool, 1, &descSet); return r; }
        memcpy(mapped, host_data, host_size);
        vkUnmapMemory(ctx->device, ctx->stagingMemory);

        dbi.buffer = ctx->stagingBuffer;
        dbi.offset = head;
        dbi.range = host_size;
    } else {
        /* use provided GPU buffer */
        if (src_buffer == VK_NULL_HANDLE) {
            logging_error("Neither host_data nor src_buffer provided");
            vkFreeDescriptorSets(ctx->device, ctx->descriptorPool, 1, &descSet);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        dbi.buffer = src_buffer;
        dbi.offset = 0;
        dbi.range = VK_WHOLE_SIZE;
    }

    VkWriteDescriptorSet writes[2];
    memset(writes, 0, sizeof(writes));

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descSet;
    writes[0].dstBinding = BINDING_SRC_BUFFER;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &dbi;

    VkDescriptorImageInfo dii = { .imageView = dst_view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descSet;
    writes[1].dstBinding = BINDING_DST_IMAGE;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &dii;

    vkUpdateDescriptorSets(ctx->device, 2, writes, 0, NULL);

    /* Bind and dispatch */
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->pipelineLayout, 0, 1, &descSet, 0, NULL);

    uint32_t push[4];
    push[0] = (uint32_t)(dbi.offset & 0xffffffffu);
    push[1] = (uint32_t)(dbi.range & 0xffffffffu);
    push[2] = extent.width;
    push[3] = extent.height;
    vkCmdPushConstants(cmd, ctx->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);

    uint32_t gx = (extent.width + XCLIPSE_LOCAL_X - 1) / XCLIPSE_LOCAL_X;
    uint32_t gy = (extent.height + XCLIPSE_LOCAL_Y - 1) / XCLIPSE_LOCAL_Y;
    uint32_t gz = extent.depth ? extent.depth : 1;
    vkCmdDispatch(cmd, gx, gy, gz);

    /* Free descriptor set (using FREE_DESCRIPTOR_SET pool) */
    vkFreeDescriptorSets(ctx->device, ctx->descriptorPool, 1, &descSet);

    return VK_SUCCESS;
}

/* Helpers */

static VkResult create_descriptor_layouts(VkDevice dev, VkDescriptorSetLayout *outDsl, VkPipelineLayout *outPl)
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

    VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };
    VkResult r = vkCreateDescriptorSetLayout(dev, &dslci, NULL, outDsl);
    if (r != VK_SUCCESS) return r;

    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 4 * sizeof(uint32_t) };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = outDsl,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcr
    };
    return vkCreatePipelineLayout(dev, &plci, NULL, outPl);
}

static VkResult create_descriptor_pool(VkDevice dev, VkDescriptorPool *outPool)
{
    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 1024;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1024;

    VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO_SUCCESS;
}

/* Helpers */

static VkResult create_descriptor_layouts(VkDevice dev, VkDescriptorSetLayout *outDsl, VkPipelineLayout *outPl)
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

    VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };
    VkResult r = vkCreateDescriptorSetLayout(dev, &dslci, NULL, outDsl);
    if (r != VK_SUCCESS) return r;

    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 4 * sizeof(uint32_t) };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = outDsl,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcr
    };
    return vkCreatePipelineLayout(dev, &plci, NULL, outPl);
}

static VkResult create_descriptor_pool(VkDevice dev, VkDescriptorPool *outPool)
{
    VkDescriptorPoolSize poolSizes[2];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 1024;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 1024;

    VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1024,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    };
    return vkCreateDescriptorPool(dev, &dpci, NULL, outPool);
}

static VkResult create_shader_module(VkDevice dev, const uint32_t *words, size_t size, VkShaderModule *outModule)
{
    if (!words || size == 0) { *outModule = VK_NULL_HANDLE; return VK_ERROR_INITIALIZATION_FAILED; }
    VkShaderModuleCreateInfo smci = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = words };
    return vkCreateShaderModule(dev, &smci, NULL, outModule);
}

static VkResult create_compute_pipeline(VkDevice dev, VkPipelineLayout layout, VkShaderModule module, VkPipeline *outPipeline)
{
    VkSpecializationMapEntry mapEntries[2];
    mapEntries[0].constantID = 0;
    mapEntries[0].offset = 0;
    mapEntries[0].size = sizeof(uint32_t);
    mapEntries[1].constantID = 1;
    mapEntries[1].offset = sizeof(uint32_t);
    mapEntries[1].size = sizeof(uint32_t);

    uint32_t specData[2] = { XCLIPSE_LOCAL_X, XCLIPSE_LOCAL_Y };
    VkSpecializationInfo spec = { .mapEntryCount = 2, .pMapEntries = mapEntries, .dataSize = sizeof(specData), .pData = specData };

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = &spec
    };

    VkComputePipelineCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };
    return vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, outPipeline);
}

static VkResult init_staging_pool(VkDevice device, VkPhysicalDevice physical, VkBuffer *outBuf, VkDeviceMemory *outMem, size_t pool_size)
{
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = pool_size, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    VkResult r = vkCreateBuffer(device, &bci, NULL, outBuf);
    if (r != VK_SUCCESS) return r;

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, *outBuf, &mr);

    VkPhysicalDeviceMemoryProperties pr;
    vkGetPhysicalDeviceMemoryProperties(physical, &pr);

    uint32_t mem_idx = UINT32_MAX;
    for (uint32_t i = 0; i < pr.memoryTypeCount; ++i) {
        uint32_t mask = (1u << i);
        if ((mr.memoryTypeBits & mask) == 0) continue;
        VkMemoryPropertyFlags props = pr.memoryTypes[i].propertyFlags;
        if ((props & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            mem_idx = i;
            break;
        }
    }
    if (mem_idx == UINT32_MAX) { vkDestroyBuffer(device, *outBuf, NULL); return VK_ERROR_MEMORY_MAP_FAILED; }

    VkMemoryAllocateInfo mai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = mem_idx };
    r = vkAllocateMemory(device, &mai, NULL, outMem);
    if (r != VK_SUCCESS) { vkDestroyBuffer(device, *outBuf, NULL); return r; }

    r = vkBindBufferMemory(device, *outBuf, *outMem, 0);
    if (r != VK_SUCCESS) { vkFreeMemory(device, *outMem, NULL); vkDestroyBuffer(device, *outBuf, NULL); return r; }

    return VK_SUCCESS;
}
