#pragma once

#include "photon/photon_device.h"
#include <vulkan/vulkan_core.h>

typedef struct PhDevice {
    VkPhysicalDevice           physDevice;
    VkDevice                   device;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures   features;
    VkQueue                    graphicsQueue;
    VkQueue                    computeQueue;
    VkQueue                    transferQueue;
    VkSwapchainKHR             swapchain;
    VkImage                   *pSwapchainImages;
    VkImageView               *pSwapchainImageViews;
    uint32_t                   swapchainImageCount;
    VkFormat                   swapchainFormat;
    VkExtent2D                 swapchainExtent;
} PhDevice;