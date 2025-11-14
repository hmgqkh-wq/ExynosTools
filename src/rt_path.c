#include "logging.h"
#include "rt_path.h"
#include <string.h>

// Minimal, safe scaffolding for RT. Full AS/SBT setup requires raygen/miss/hit SPIR-V and device addresses.
// This keeps linkage intact and enables vkCmdTraceRaysKHR calls when wired.

static VkDeviceAddress get_buffer_address(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
    PFN_vkGetBufferDeviceAddressKHR getAddr = (PFN_vkGetBufferDeviceAddressKHR) vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
    if (!getAddr) return 0;
    return getAddr(device, &info);
}

VkResult xeno_rt_init(VkDevice device, VkPhysicalDevice phys, XenoRT* out) {
    if (!out) return VK_ERROR_INITIALIZATION_FAILED;
    memset(out, 0, sizeof(*out));

#ifdef ENABLE_RAYTRACING
    // In a complete implementation:
    // 1) Build BLAS/TLAS
    // 2) Create SBT (raygen/miss/hit records)
    // 3) Create rtPipeline and rtLayout
    // For now, mark ready and allow dispatch call to proceed (no-op).
    XENO_LOGI("rt_path: initialized (scaffold ready)");
    out->ready = 1;
#endif

    return VK_SUCCESS;
}

void xeno_rt_destroy(VkDevice device, XenoRT* rt) {
    if (!rt) return;
#ifdef ENABLE_RAYTRACING
    if (rt->rtPipeline) vkDestroyPipeline(device, rt->rtPipeline, NULL);
    if (rt->rtLayout)   vkDestroyPipelineLayout(device, rt->rtLayout, NULL);
    if (rt->sbtBuffer)  vkDestroyBuffer(device, rt->sbtBuffer, NULL);
    if (rt->sbtMemory)  vkFreeMemory(device, rt->sbtMemory, NULL);
    if (rt->asScratch)  vkDestroyBuffer(device, rt->asScratch, NULL);
    if (rt->asScratchMem) vkFreeMemory(device, rt->asScratchMem, NULL);
    if (rt->tlas) {
        PFN_vkDestroyAccelerationStructureKHR destroyAS =
            (PFN_vkDestroyAccelerationStructureKHR) vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
        if (destroyAS) destroyAS(device, rt->tlas, NULL);
    }
    if (rt->blas) {
        PFN_vkDestroyAccelerationStructureKHR destroyAS =
            (PFN_vkDestroyAccelerationStructureKHR) vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
        if (destroyAS) destroyAS(device, rt->blas, NULL);
    }
    XENO_LOGI("rt_path: destroyed");
#endif
}

VkResult xeno_rt_dispatch(VkCommandBuffer cmd, XenoRT* rt, uint32_t width, uint32_t height) {
#ifdef ENABLE_RAYTRACING
    if (!rt || !rt->ready) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkCmdTraceRaysKHR trace =
        (PFN_vkCmdTraceRaysKHR) vkGetDeviceProcAddr((VkDevice)0, "vkCmdTraceRaysKHR"); // device not needed for cmd call; kept for form
    // In scaffold mode, we log; full implementation would bind pipeline and regions and call vkCmdTraceRaysKHR.
    XENO_LOGD("rt_path: trace rays requested %ux%u (scaffold)", width, height);
#endif
    return VK_SUCCESS;
}
