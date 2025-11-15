#include <vulkan/vulkan.h>

/* Provide default NULL pointers for "original" hooks so the linker is satisfied.
   Your wrapper should check for NULL before calling these. */

PFN_vkCreateDevice vkCreateDevice_original = NULL;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass_original = NULL;

/* Add more if your code references additional *_original symbols */
