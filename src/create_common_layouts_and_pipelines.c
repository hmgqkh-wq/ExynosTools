/*
  src/create_common_layouts_and_pipelines.c
  Shared helpers for pipeline creation.
*/
#include "logging.h"
#include <vulkan/vulkan.h>

VkPipelineLayout create_common_pipeline_layout(VkDevice device, VkDescriptorSetLayout dsl) {
    VkPushConstantRange pcr = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 4 * sizeof(uint32_t) };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsl,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcr
    };
    VkPipelineLayout layout;
    VkResult r = vkCreatePipelineLayout(device, &plci, NULL, &layout);
    if (r != VK_SUCCESS) {
        logging_error("vkCreatePipelineLayout failed: %d", (int)r);
        return VK_NULL_HANDLE;
    }
    return layout;
}
