#pragma once

#include "photon_status.h"
#include <vulkan/vulkan.h>

typedef struct PhDevice *PhDeviceHandle;
typedef VkShaderModule PhShaderModule;

PhStatus ph_create_shader_module(PhDeviceHandle hDevice, const char *path, PhShaderModule *out);
PhStatus ph_destroy_shader_module(PhDeviceHandle hDevice, PhShaderModule module);
