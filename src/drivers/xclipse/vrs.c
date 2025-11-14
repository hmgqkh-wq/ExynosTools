#include <vulkan/vulkan.h>

void apply_vrs(VkCommandBuffer cmd, VkExtent2D extent) {
    VkFragmentShadingRateKHR rate = {2, 2};  // 2x2 shading for low-detail, FPS boost
    vkCmdSetFragmentShadingRateKHR(cmd, &rate, NULL);
}
