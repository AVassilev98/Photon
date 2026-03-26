#pragma once

#include "photon_instance.h"
#include "photon_window.h"
#include "foundation/span.h"

typedef struct PhDevice *PhDeviceHandle;
typedef VkCommandBuffer PhCommandBuffer;
typedef VkSubmitInfo    PhQueueSubmitInfo;

typedef struct PhImage {
    VkImageView defaultView;
    VkImage     image;
    VkExtent2D  extent;
    VkSemaphore readySemaphore;
} PhImage;
typedef VkSemaphore     PhSemaphore;

typedef enum PhQueueType {
    PH_QUEUE_TYPE_GRAPHICS,
    PH_QUEUE_TYPE_COMPUTE,
    PH_QUEUE_TYPE_TRANSFER,
} PhQueueType;

typedef struct PhCapability {
    uint32_t discrete               : 1;
    uint32_t rtCapable              : 1;
    uint32_t swapchain              : 1;
    uint32_t graphicsQueue          : 1;
    uint32_t asyncComputeQueue      : 1;
    uint32_t dedicatedTransfer      : 1;
    uint32_t samplerAnisotropy      : 1;
    uint32_t fillModeNonSolid       : 1;
    uint32_t wideLines              : 1;
    uint32_t largePoints            : 1;
    uint32_t multiDrawIndirect      : 1;
    uint32_t shaderInt64            : 1;
    uint32_t shaderFloat64          : 1;
    uint32_t bufferDeviceAddress    : 1;
    uint32_t descriptorIndexing     : 1;
    uint32_t timelineSemaphore      : 1;
    uint32_t dynamicRendering       : 1;
    uint32_t extendedDynamicState   : 1;
    uint32_t synchronization2       : 1;
    PhExtent2D minimumImageDimensions;
    uint32_t   minPushConstantsSize;
    uint64_t   minimumVRAM;
} PhCapability;

typedef VkSurfaceFormatKHR PhSurfaceFormat;
typedef VkPresentModeKHR PhPresentMode;

typedef struct PhPresentOptions {
    PhSurfaceFormat format;
    PhPresentMode mode;
} PhPresentOptions;

typedef struct PhDeviceInfo {
    char *pName;
    PhDeviceHandle handle;
    PhCapability capabilities;
} PhDeviceInfo;
FDN_SPAN_DEFINE(PhDeviceInfo, PhDeviceInfoSpan)

struct PhPipeline;

PhStatus ph_enumerate_devices(PhInstanceHandle hInstance, PhCapability caps, PhDeviceInfoSpan *ppDeviceInfo);
PhStatus ph_configure_device_for_present(PhDeviceHandle hDevice, PhSurfaceHandle hSurface, PhPresentOptions opts);
PhStatus ph_device_command_buffer_create(PhDeviceHandle hDevice, PhQueueType type, size_t count, PhCommandBuffer *pBuffers);
PhStatus ph_device_command_buffer_destroy(PhDeviceHandle hDevice, PhQueueType type, size_t count, PhCommandBuffer *pBuffers);
PhStatus ph_device_semaphore_create(PhDeviceHandle hDevice, PhSemaphore *out);
PhStatus ph_device_semaphore_destroy(PhDeviceHandle hDevice, PhSemaphore sem);
PhStatus ph_device_present_image_get_next(PhDeviceHandle hDevice, PhImage *image);
PhStatus ph_device_queue_submit(PhDeviceHandle handle, PhQueueType type, PhQueueSubmitInfo *submitInfo);
PhStatus ph_device_present(PhDeviceHandle hDevice, PhSemaphore *pWaitSemaphores, size_t numSemaphores);
