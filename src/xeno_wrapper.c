// src/xeno_wrapper.c
// Vulkan dispatch wrapper and compatibility shims for Xclipse-940
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>      // declares getpid()
#include <vulkan/vulkan.h>
#include "logging.h"

static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static void* g_next_lib = NULL;

static PFN_vkGetInstanceProcAddr real_vkGetInstanceProcAddr = NULL;
static PFN_vkGetDeviceProcAddr real_vkGetDeviceProcAddr = NULL;

static void ensure_real_procs(void) {
    if (g_next_lib) return;
    g_next_lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_next_lib) {
        real_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(RTLD_NEXT, "vkGetInstanceProcAddr");
        real_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)dlsym(RTLD_NEXT, "vkGetDeviceProcAddr");
    } else {
        real_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(g_next_lib, "vkGetInstanceProcAddr");
        real_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)dlsym(g_next_lib, "vkGetDeviceProcAddr");
    }
    if (!real_vkGetInstanceProcAddr || !real_vkGetDeviceProcAddr) {
        XENO_LOGE("xeno_wrapper: failed to resolve real vkGet*ProcAddr");
    } else {
        XENO_LOGI("xeno_wrapper: resolved real Vulkan loader");
    }
}

typedef struct PipelineCacheEntry {
    VkDevice device;
    uint64_t key_hash;
    VkPipeline pipeline;
    struct PipelineCacheEntry* next;
} PipelineCacheEntry;

static PipelineCacheEntry* g_pipeline_cache = NULL;
static pthread_mutex_t g_pipeline_cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static void cache_pipeline_add(VkDevice device, uint64_t key, VkPipeline p) {
    PipelineCacheEntry* e = (PipelineCacheEntry*)malloc(sizeof(PipelineCacheEntry));
    if (!e) return;
    e->device = device;
    e->key_hash = key;
    e->pipeline = p;
    pthread_mutex_lock(&g_pipeline_cache_mutex);
    e->next = g_pipeline_cache;
    g_pipeline_cache = e;
    pthread_mutex_unlock(&g_pipeline_cache_mutex);
}

static VkPipeline cache_pipeline_lookup(VkDevice device, uint64_t key) {
    pthread_mutex_lock(&g_pipeline_cache_mutex);
    PipelineCacheEntry* cur = g_pipeline_cache;
    while (cur) {
        if (cur->device == device && cur->key_hash == key) {
            pthread_mutex_unlock(&g_pipeline_cache_mutex);
            return cur->pipeline;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_pipeline_cache_mutex);
    return VK_NULL_HANDLE;
}

static uint64_t hash_bytes(const void* data, size_t len) {
    const unsigned char* p = (const unsigned char*)data;
    uint64_t h = 1469598103934665603ULL; // FNV-1a 64
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static PFN_vkVoidFunction my_vkGetInstanceProcAddr(VkInstance instance, const char* pName);
static PFN_vkVoidFunction my_vkGetDeviceProcAddr(VkDevice device, const char* pName);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    pthread_once(&g_init_once, ensure_real_procs);
    if (!pName) return NULL;

    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) return (PFN_vkVoidFunction) my_vkGetDeviceProcAddr;
    if (strcmp(pName, "vkCreateShaderModule") == 0) return (PFN_vkVoidFunction) vkCreateShaderModule;
    if (strcmp(pName, "vkCreateComputePipelines") == 0) return (PFN_vkVoidFunction) vkCreateComputePipelines;
    if (strcmp(pName, "vkEnumeratePhysicalDevices") == 0) return (PFN_vkVoidFunction) vkEnumeratePhysicalDevices;
    if (strcmp(pName, "vkGetPhysicalDeviceProperties2") == 0) return (PFN_vkVoidFunction) vkGetPhysicalDeviceProperties2;
    if (strcmp(pName, "vkGetPhysicalDeviceFeatures2") == 0) return (PFN_vkVoidFunction) vkGetPhysicalDeviceFeatures2;

    if (real_vkGetInstanceProcAddr) {
        return real_vkGetInstanceProcAddr(instance, pName);
    }
    return NULL;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    return my_vkGetDeviceProcAddr(device, pName);
}

static PFN_vkVoidFunction my_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    pthread_once(&g_init_once, ensure_real_procs);
    if (!pName) return NULL;

    if (strcmp(pName, "vkCreateGraphicsPipelines") == 0) return (PFN_vkVoidFunction) vkCreateGraphicsPipelines;
    if (strcmp(pName, "vkCreateComputePipelines") == 0) return (PFN_vkVoidFunction) vkCreateComputePipelines;
    if (strcmp(pName, "vkCreateShaderModule") == 0) return (PFN_vkVoidFunction) vkCreateShaderModule;

    if (real_vkGetDeviceProcAddr) {
        return real_vkGetDeviceProcAddr(device, pName);
    }
    return NULL;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo,
                                                    const VkAllocationCallbacks* pAllocator, VkShaderModule* pModule) {
    if (!pCreateInfo || !pCreateInfo->pCode || (pCreateInfo->codeSize % 4) != 0) {
        XENO_LOGE("vkCreateShaderModule: invalid SPIR-V codeSize %zu", (size_t)pCreateInfo->codeSize);
        return VK_ERROR_INVALID_SHADER_NV;
    }

    PFN_vkCreateShaderModule real = (PFN_vkCreateShaderModule)real_vkGetDeviceProcAddr(device, "vkCreateShaderModule");
    if (!real) {
        XENO_LOGE("vkCreateShaderModule: real implementation not found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return real(device, pCreateInfo, pAllocator, pModule);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                        uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos,
                                                        const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    PFN_vkCreateComputePipelines real = (PFN_vkCreateComputePipelines)real_vkGetDeviceProcAddr(device, "vkCreateComputePipelines");
    if (!real) {
        XENO_LOGE("vkCreateComputePipelines: real implementation not found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (uint32_t i = 0; i < createInfoCount; ++i) {
        const VkComputePipelineCreateInfo* ci = &pCreateInfos[i];
        uint64_t key = 1469598103934665603ULL;
        if (ci->stage.module) {
            key ^= (uint64_t)(uintptr_t)ci->stage.module;
            key *= 1099511628211ULL;
        }
        if (ci->layout) {
            key ^= (uint64_t)(uintptr_t)ci->layout;
            key *= 1099511628211ULL;
        }
        if (ci->stage.pSpecializationInfo && ci->stage.pSpecializationInfo->dataSize > 0) {
            key ^= hash_bytes(ci->stage.pSpecializationInfo->pData, ci->stage.pSpecializationInfo->dataSize);
            key *= 1099511628211ULL;
        }

        VkPipeline cached = cache_pipeline_lookup(device, key);
        if (cached != VK_NULL_HANDLE) {
            pPipelines[i] = cached;
            continue;
        }

        VkPipeline outPipeline = VK_NULL_HANDLE;
        VkResult r = real(device, pipelineCache, 1, ci, pAllocator, &outPipeline);
        if (r == VK_SUCCESS && outPipeline != VK_NULL_HANDLE) {
            cache_pipeline_add(device, key, outPipeline);
            pPipelines[i] = outPipeline;
        } else {
            return r;
        }
    }

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount,
                                                          VkPhysicalDevice* pPhysicalDevices) {
    PFN_vkEnumeratePhysicalDevices real = (PFN_vkEnumeratePhysicalDevices)real_vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    if (!real) {
        XENO_LOGE("vkEnumeratePhysicalDevices: real implementation not found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return real(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    PFN_vkGetPhysicalDeviceProperties2 real = (PFN_vkGetPhysicalDeviceProperties2)real_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkGetPhysicalDeviceProperties2");
    if (!real) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        if (pProperties) {
            pProperties->properties = props;
        }
        return;
    }
    real(physicalDevice, pProperties);

    strncpy(pProperties->properties.deviceName, "Xclipse-940 (Compat)", VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1);

    VkPhysicalDeviceLimits* L = &pProperties->properties.limits;
    if (L->maxDescriptorSetStorageBuffers < 64) L->maxDescriptorSetStorageBuffers = 64;
    if (L->maxComputeWorkGroupInvocations < 256) L->maxComputeWorkGroupInvocations = 256;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    PFN_vkGetPhysicalDeviceFeatures2 real = (PFN_vkGetPhysicalDeviceFeatures2)real_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkGetPhysicalDeviceFeatures2");
    if (!real) {
        VkPhysicalDeviceFeatures f;
        vkGetPhysicalDeviceFeatures(physicalDevice, &f);
        if (pFeatures) pFeatures->features = f;
        return;
    }
    real(physicalDevice, pFeatures);

    pFeatures->features.textureCompressionETC2 = VK_TRUE;
}

static void __attribute__((constructor)) wrapper_init(void) {
    pthread_once(&g_init_once, ensure_real_procs);
    XENO_LOGI("xeno_wrapper: initialized wrapper (pid=%d)", getpid());
}

static void __attribute__((destructor)) wrapper_fini(void) {
    XENO_LOGI("xeno_wrapper: shutting down wrapper");
    pthread_mutex_lock(&g_pipeline_cache_mutex);
    PipelineCacheEntry* cur = g_pipeline_cache;
    while (cur) {
        PipelineCacheEntry* n = cur->next;
        free(cur);
        cur = n;
    }
    g_pipeline_cache = NULL;
    pthread_mutex_unlock(&g_pipeline_cache_mutex);
}
