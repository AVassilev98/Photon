#pragma once

#include "foundation/bitvec.h"
#include "foundation/vec.h"
#include "photon/photon_device.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include "vk_mem_alloc.h"

#define KB 1024
#define MB (KB * KB)
#define GB (MB * KB)
#define PH_ALLOCATION_GRANULARITY (4 * KB)

typedef struct PhTransfer {
    VkFence         transferComplete;
    VkCommandBuffer commandBuffer;
    uint32_t        startChunk;
    uint32_t        numChunks;
} PhTransfer;

FDN_BITVEC_DEFINE(StagingBufferFreeBitVec);
FDN_VEC_DEFINE(PhTransfer, PhTransferVector);

typedef struct PhDevice {
    VkPhysicalDevice           physDevice;
    VkDevice                   device;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures   features;
    uint32_t                   graphicsQueueFamily;
    uint32_t                   computeQueueFamily;
    uint32_t                   transferQueueFamily;
    VkQueue                    graphicsQueue;
    VkQueue                    computeQueue;
    VkQueue                    transferQueue;
    VkCommandPool              graphicsPool;
    VkCommandPool              computePool;
    VkCommandPool              transferPool;
    VkSwapchainKHR             swapchain;
    VkImage                   *pSwapchainImages;
    VkImageView               *pSwapchainImageViews;
    VkSemaphore               *pPresentSemaphores;
    VkSemaphore               *pRenderSemaphores;
    VkFence                   *pPresentFences;
    uint32_t                   swapchainImageCount;
    VkFormat                   swapchainFormat;
    VkExtent2D                 swapchainExtent;
    size_t                     frame;

    VkInstance                 instance;
    VmaAllocator               allocator;

    PhBuffer                   stagingBuffer;
    StagingBufferFreeBitVec    stagingBufferAllocations;
    PhTransferVector           activeTransfers;
} PhDevice;