// src/rt_path.c
// Full drop-in replacement for rt_path.c
// - Eliminates unused-function warnings by providing a complete module
// - Provides helper utilities used by other modules (buffer address, device address helpers)
// - Keeps implementations explicit and self-contained for C99 builds

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include "rt_path.h"
#include "logging.h"

/*
  This file exposes:
    - get_buffer_device_address: returns VkDeviceAddress for a buffer
    - rt_create_buffer_with_memory: convenience to create VkBuffer + allocate device memory
    - rt_destroy_buffer_with_memory: cleanup
  It intentionally avoids static unused functions and provides full definitions to silence warnings.
*/

/* Query device address for a buffer (requires VK_KHR_buffer_device_address or core Vulkan 1.2) */
VkDeviceAddress get_buffer_device_address(VkDevice device, VkBuffer buffer)
{
    if (device == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE) return (VkDeviceAddress)0;

    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = NULL,
        .buffer = buffer
    };
    /* vkGetBufferDeviceAddress may be NULL if extension not enabled; guard call */
    PFN_vkGetBufferDeviceAddress fn = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress");
    if (fn) {
        return fn(device, &info);
    } else {
        /* fallback: zero address */
        XENO_LOGW("rt_path: vkGetBufferDeviceAddress not available on device; returning 0");
        return (VkDeviceAddress)0;
    }
}

/* Create a buffer and allocate device memory for it with requested usage and properties.
   Returns VK_SUCCESS on success and fills out buffer and memory. */
VkResult rt_create_buffer_with_memory(VkDevice device, VkPhysicalDevice physical,
                                      VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties,
                                      VkBuffer *outBuffer, VkDeviceMemory *outMemory)
{
    if (!device || !outBuffer || !outMemory) return VK_ERROR_INITIALIZATION_FAILED;

    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL
    };

    VkResult res = vkCreateBuffer(device, &bci, NULL, outBuffer);
    if (res != VK_SUCCESS) {
        XENO_LOGE("rt_path: vkCreateBuffer failed: %d", res);
        return res;
    }

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, *outBuffer, &mr);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    uint32_t memIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((mr.memoryTypeBits & (1u << i)) &&
            ( (memProps.memoryTypes[i].propertyFlags & properties) == properties )) {
            memIndex = i;
            break;
        }
    }

    if (memIndex == UINT32_MAX) {
        XENO_LOGE("rt_path: no suitable memory type found for buffer allocation");
        vkDestroyBuffer(device, *outBuffer, NULL);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mr.size,
        .memoryTypeIndex = memIndex
    };

    res = vkAllocateMemory(device, &mai, NULL, outMemory);
    if (res != VK_SUCCESS) {
        XENO_LOGE("rt_path: vkAllocateMemory failed: %d", res);
        vkDestroyBuffer(device, *outBuffer, NULL);
        return res;
    }

    res = vkBindBufferMemory(device, *outBuffer, *outMemory, 0);
    if (res != VK_SUCCESS) {
        XENO_LOGE("rt_path: vkBindBufferMemory failed: %d", res);
        vkFreeMemory(device, *outMemory, NULL);
        vkDestroyBuffer(device, *outBuffer, NULL);
        return res;
    }

    return VK_SUCCESS;
}

/* Destroy buffer + free memory pair created by rt_create_buffer_with_memory */
void rt_destroy_buffer_with_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
    if (device == VK_NULL_HANDLE) return;
    if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, NULL);
    if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, NULL);
}

/* Helper to map memory, copy data and optionally flush if non-coherent */
VkResult rt_upload_to_buffer(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, const void *data, VkDeviceSize size, VkPhysicalDevice physical)
{
    if (!device || memory == VK_NULL_HANDLE || !data || size == 0) return VK_ERROR_INITIALIZATION_FAILED;

    void *mapped = NULL;
    VkResult res = vkMapMemory(device, memory, offset, size, 0, &mapped);
    if (res != VK_SUCCESS) {
        XENO_LOGE("rt_path: vkMapMemory failed: %d", res);
        return res;
    }

    memcpy(mapped, data, (size_t)size);
    vkUnmapMemory(device, memory);

    /* If memory is not host-coherent, flush is required; try to detect */
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physical, &memProps);

    /* Best-effort; caller may ensure HOST_COHERENT flag on allocation to skip flush */
    /* We do not call vkFlushMappedMemoryRanges here as it requires tracking ranges and non-coherent bit knowledge.
       If necessary, callers should allocate HOST_COHERENT memory or perform explicit flush. */

    (void)memProps;
    return VK_SUCCESS;
}

/* Utility: returns number of bytes for a given buffer usage heuristic (for staging sizing) */
size_t rt_guess_staging_size(VkDeviceSize requested)
{
    /* Constrain between 64KiB and 64MiB for safety */
    const size_t MIN = 64u * 1024u;
    const size_t MAX = 64u * 1024u * 1024u;
    size_t s = (size_t)requested;
    if (s < MIN) s = MIN;
    if (s > MAX) s = MAX;
    return s;
}

/* Expose a debug print of a device address (useful for logs) */
void rt_log_buffer_address(VkDevice device, VkBuffer buffer)
{
    VkDeviceAddress addr = get_buffer_device_address(device, buffer);
    XENO_LOGD("rt_path: buffer %p device address = 0x%016llx", (void*)buffer, (unsigned long long)addr);
}

/* Provide full function prototypes for header linkage (repeat in rt_path.h) */
#ifdef RT_PATH_IMPLEMENTATION_TEST
/* Minimal test harness (not used in production) */
#include <assert.h>
int rt_self_test(VkDevice device, VkPhysicalDevice physical) {
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkResult r = rt_create_buffer_with_memory(device, physical, 4096, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf, &mem);
    if (r != VK_SUCCESS) return -1;
    rt_destroy_buffer_with_memory(device, buf, mem);
    return 0;
}
#endif

/* End of src/rt_path.c */
