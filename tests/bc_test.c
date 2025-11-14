#include "bc_emulate.h"
#include <vulkan/vulkan.h>
#include <assert.h>

int main() {
  VkDevice device = /* initialize Vulkan device */;
  VkPhysicalDevice phys = /* initialize physical device */;
  XenoBCContext* ctx = xeno_bc_create_context(device, phys);
  assert(ctx != NULL);
  // Test decoding
  VkBuffer src_bc = /* create test BC buffer */;
  VkImage dst_rgba = /* create test image */;
  VkCommandBuffer cmd = /* create command buffer */;
  VkExtent3D extent = {256, 256, 1};
  VkResult res = xeno_bc_decode_image(cmd, ctx, src_bc, dst_rgba, XENO_BC4, extent);
  assert(res == VK_SUCCESS);
  xeno_bc_destroy_context(ctx);
  return 0;
}
