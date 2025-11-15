// src/rt_path.c
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "rt_path.h"
#include "xeno_log.h"

/* Query device address for a buffer */
VkDeviceAddress get_buffer_device_address(VkDevice device, VkBuffer buffer)
{
    if (device == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE) return (VkDeviceAddress)0;

    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = NULL,
        .buffer = buffer
    };
    PFN_vkGetBufferDeviceAddress fn = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress");
    if (fn) {
        return fn(device, &info);
    } else {
        XENO_LOGW("rt_path: vkGetBufferDeviceAddress not available; returning 0");
        return (VkDeviceAddress)0;
    }
}

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
            ((memProps.memoryTypes[i].propertyFlags & properties) == properties)) {
            memIndex = i;
            break;
        }
    }

    if (memIndex == UINT32_MAX) {
        XENO_LOGE("rt_path: no suitable memory type found");
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

void rt_destroy_buffer_with_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
    if (device == VK_NULL_HANDLE) return;
    if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, NULL);
    if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, NULL);
}

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

    return VK_SUCCESS;
}

size_t rt_guess_staging_size(VkDeviceSize requested)
{
    const size_t MIN = 64u * 1024u;
    const size_t MAX = 64u * 1024u * 1024u;
    size_t s = (size_t)requested;
    if (s < MIN) s = MIN;
    if (s > MAX) s = MAX;
    return s;
}

void rt_log_buffer_address(VkDevice device, VkBuffer buffer)
{
    VkDeviceAddress addr = get_buffer_device_address(device, buffer);
    XENO_LOGD("rt_path: buffer %p device address = 0x%016llx", (void*)buffer, (unsigned long long)addr);
}
