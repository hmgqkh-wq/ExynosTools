/*
  src/detect.c
  Device detection helpers.
*/
#include "logging.h"
#include <vulkan/vulkan.h>

int detect_xclipse940(VkPhysicalDevice physical) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical, &props);
    if (props.deviceID == 0x940) {
        logging_info("Detected Xclipse 940 GPU");
        return 1;
    }
    return 0;
}
