#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include "logging.h"
#include "xeno_wrapper.h"

static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static void* g_next_lib = NULL;

static PFN_vkGetInstanceProcAddr real_vkGetInstanceProcAddr = NULL;
static PFN_vkGetDeviceProcAddr   real_vkGetDeviceProcAddr   = NULL;

static void ensure_real_procs(void) {
    if (g_next_lib) return;
    g_next_lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!g_next_lib) {
        real_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(RTLD_NEXT, "vkGetInstanceProcAddr");
        real_vkGetDeviceProcAddr   = (PFN_vkGetDeviceProcAddr)  dlsym(RTLD_NEXT, "vkGetDeviceProcAddr");
    } else {
        real_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(g_next_lib, "vkGetInstanceProcAddr");
        real_vkGetDeviceProcAddr   = (PFN_vkGetDeviceProcAddr)  dlsym(g_next_lib, "vkGetDeviceProcAddr");
    }
    if (!real_vkGetInstanceProcAddr || !real_vkGetDeviceProcAddr)
        XENO_LOGE("xeno_wrapper: failed to resolve real vkGet*ProcAddr");
}

uint32_t xeno_wrapper_get_caps(void) {
    return XENO_CAP_PIPELINE_CACHE_PERSIST |
           XENO_CAP_DESCRIPTOR_REUSE |
           XENO_CAP_FEATURE_NORMALIZATION |
           XENO_CAP_BC_DECODE_COMPUTE |
           XENO_CAP_SPECIALIZATION_CONSTANTS |
           XENO_CAP_ASYNC_PIPELINE_CREATION |
           XENO_CAP_SPIRV_VALIDATION |
           XENO_CAP_BINDLESS_DESCRIPTOR |
           XENO_CAP_RAYTRACING_SCAFFOLD;
}

VkResult xeno_wrapper_validate_spirv(const uint32_t* words, uint32_t byte_len) {
    if (!words || (byte_len % 4) != 0) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    if (words[0] != 0x07230203) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return VK_SUCCESS;
}

VkResult xeno_wrapper_save_pipeline_cache(VkDevice device, VkPipelineCache cache, const char* path) {
    PFN_vkGetPipelineCacheData getData = (PFN_vkGetPipelineCacheData) vkGetDeviceProcAddr(device, "vkGetPipelineCacheData");
    if (!getData || !path) return VK_ERROR_INITIALIZATION_FAILED;
    size_t size = 0;
    if (getData(device, cache, &size, NULL) != VK_SUCCESS || size == 0) return VK_ERROR_INITIALIZATION_FAILED;
    void* buf = malloc(size);
    if (!buf) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult r = getData(device, cache, &size, buf);
    if (r == VK_SUCCESS) {
        FILE* f = fopen(path, "wb");
        if (!f) r = VK_ERROR_INITIALIZATION_FAILED;
        else { fwrite(buf, 1, size, f); fclose(f); }
    }
    free(buf);
    return r;
}

VkResult xeno_wrapper_load_pipeline_cache(VkDevice device, const char* path, VkPipelineCache* out_cache) {
    if (!path || !out_cache) return VK_ERROR_INITIALIZATION_FAILED;
    FILE* f = fopen(path, "rb");
    if (!f) return VK_ERROR_INITIALIZATION_FAILED;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    void* buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return VK_ERROR_OUT_OF_HOST_MEMORY; }
    fread(buf, 1, (size_t)sz, f); fclose(f);

    VkPipelineCacheCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, .initialDataSize = (size_t)sz, .pInitialData = buf };
    VkResult r = vkCreatePipelineCache(device, &ci, NULL, out_cache);
    free(buf);
    return r;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    pthread_once(&g_init_once, ensure_real_procs);
    if (real_vkGetInstanceProcAddr) return real_vkGetInstanceProcAddr(instance, pName);
    return NULL;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    pthread_once(&g_init_once, ensure_real_procs);
    if (real_vkGetDeviceProcAddr) return real_vkGetDeviceProcAddr(device, pName);
    return NULL;
}

static void __attribute__((constructor)) wrapper_init(void) {
    pthread_once(&g_init_once, ensure_real_procs);
    XENO_LOGI("xeno_wrapper: init (pid=%d)", getpid());
}
static void __attribute__((destructor)) wrapper_fini(void) {
    XENO_LOGI("xeno_wrapper: fini");
}
