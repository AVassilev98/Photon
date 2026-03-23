#pragma once

#include "photon/photon_device.h"
#include <vulkan/vulkan_core.h>

typedef struct PhDevice {
    VkPhysicalDevice physDevice;
    VkDevice device;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
} PhDevice;