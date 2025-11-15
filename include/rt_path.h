// include/rt_path.h
#ifndef RT_PATH_H
#define RT_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <vulkan/vulkan.h>
#include <stddef.h>

/* Public buffer utilities used across the project */
VkResult rt_create_buffer_with_memory(VkDevice device, VkPhysicalDevice physical,
                                      VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties,
                                      VkBuffer *outBuffer, VkDeviceMemory *outMemory);

void rt_destroy_buffer_with_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory);

VkResult rt_upload_to_buffer(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                             const void *data, VkDeviceSize size, VkPhysicalDevice physical);

size_t rt_guess_staging_size(VkDeviceSize requested);

void rt_log_buffer_address(VkDevice device, VkBuffer buffer);

#ifdef __cplusplus
}
#endif

#endif /* RT_PATH_H */
