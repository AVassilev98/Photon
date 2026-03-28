#pragma once

#include "photon/photon_status.h"
#include "photon_instance.h"
#include "photon_window.h"
#include "foundation/span.h"
#include "vk_mem_alloc.h"

typedef struct PhDevice     *PhDeviceHandle;
typedef VkCommandBuffer      PhCommandBuffer;
typedef VkSubmitInfo         PhQueueSubmitInfo;
typedef VkBufferUsageFlags   PhBufferUsageFlags;
typedef VkSharingMode        PhSharingMode;
typedef VkDescriptorSet      PhDescriptorSet;

typedef struct PhBuffer {
    VkBuffer                  buffer;
    VmaAllocation             allocation;
    VmaAllocationCreateFlags  flags;
    void                     *hostPtr;
    uint32_t                  size;
} PhBuffer;

typedef struct PhImage {
    VkImageView defaultView;
    VkImage     image;
    VkExtent2D  extent;
    VkSemaphore readySemaphore;
} PhImage;
typedef VkSemaphore     PhSemaphore;

typedef enum PhQueueType {
    PH_QUEUE_TYPE_GRAPHICS_BIT = 0x1,
    PH_QUEUE_TYPE_COMPUTE_BIT = 0x2,
    PH_QUEUE_TYPE_TRANSFER_BIT = 0x4,
} PhQueueType;
#define PH_NUM_QUEUE_TYPES 3UL

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

typedef struct PhDescriptorWrite {
    PhDescriptorSet              set;
    uint32_t                     binding;
    uint32_t                     arrayElement;
    VkDescriptorType             type;
    uint32_t                     count;
    const VkDescriptorBufferInfo *pBufferInfo;
    const VkDescriptorImageInfo  *pImageInfo;
} PhDescriptorWrite;

typedef PhStatus (*PhPerFrameCreateFn)(PhDeviceHandle, void *userdata, uint32_t frameIndex, void *out);
typedef PhStatus (*PhPerFrameRecreateFn)(PhDeviceHandle, void *userdata, void *resource, PhExtent2D newExtent);
typedef void     (*PhPerFrameDestroyFn)(PhDeviceHandle, void *resource);

typedef struct PhPerFrameResourceCreateInfo {
    PhPerFrameCreateFn createFn;
    PhPerFrameDestroyFn destroyFn;
    // optional
    PhPerFrameRecreateFn recreateFn;
    void *recreateData;
} PhPerFrameResourceCreateInfo;
typedef uint32_t PhPerFrameResourceHandle;

struct PhPipeline;

PhStatus ph_devices_enumerate(PhInstanceHandle hInstance, PhCapability caps, PhDeviceInfoSpan *ppDeviceInfo);

PhStatus ph_device_configure_for_present(PhDeviceHandle hDevice, PhSurfaceHandle hSurface, PhPresentOptions opts);
PhStatus ph_device_swapchain_recreate(PhDeviceHandle hDevice);
PhStatus ph_device_extent_get(PhDeviceHandle hDevice, PhExtent2D *pExtent);
PhStatus ph_device_frame_index_get(PhDeviceHandle hDevice, size_t *pIndex);
PhStatus ph_device_create_staging_buffer(PhDeviceHandle hDevice, uint32_t size);
PhStatus ph_device_command_buffer_create(PhDeviceHandle hDevice, PhQueueType type, size_t count, PhCommandBuffer *pBuffers);
PhStatus ph_device_command_buffer_destroy(PhDeviceHandle hDevice, PhQueueType type, size_t count, PhCommandBuffer *pBuffers);
PhStatus ph_device_semaphore_create(PhDeviceHandle hDevice, PhSemaphore *out);
PhStatus ph_device_semaphore_destroy(PhDeviceHandle hDevice, PhSemaphore sem);
PhStatus ph_device_queue_submit(PhDeviceHandle handle, PhQueueType type, PhQueueSubmitInfo *submitInfo);
PhStatus ph_device_present_image_get_next(PhDeviceHandle hDevice, PhImage *image);
PhStatus ph_device_present(PhDeviceHandle hDevice, PhSemaphore *pWaitSemaphores, size_t numSemaphores);
PhStatus ph_device_buffer_create(PhDeviceHandle hDevice, PhQueueType queueTypeFlags, uint32_t size, PhBufferUsageFlags flags, PhSharingMode sharing, PhBuffer *out, VmaAllocationCreateFlags vmaFlags);
PhStatus ph_device_buffer_destroy(PhDeviceHandle hDevice, PhBuffer *buffer);
PhStatus ph_device_buffer_upload(PhDeviceHandle hDevice, void *cpuData, uint32_t size, PhBuffer dest);
PhStatus ph_device_buffer_map(PhDeviceHandle hDevice, PhBuffer *buffer);
PhStatus ph_device_descriptor_sets_allocate(PhDeviceHandle hDevice, const VkDescriptorSetLayout *pLayouts, uint32_t count, PhDescriptorSet *pOut);
PhStatus ph_device_descriptor_sets_free(PhDeviceHandle hDevice, PhDescriptorSet *pSets, uint32_t count);
PhStatus ph_device_descriptor_sets_write(PhDeviceHandle hDevice, const PhDescriptorWrite *pWrites, uint32_t writeCount);

PhStatus ph_device_per_frame_register(PhDeviceHandle hDevice, size_t elemSize, PhPerFrameCreateFn create, PhPerFrameDestroyFn destroy, PhPerFrameRecreateFn recreate, PhPerFrameResourceHandle *pHandle);
PhStatus ph_device_per_frame_create(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle, void *pCreateParams);
PhStatus ph_device_per_frame_destroy(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle);
PhStatus ph_device_per_frame_get(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle, void **ppOut);
PhStatus ph_device_per_frame_get_at(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle, uint32_t index, void **ppOut);
PhStatus ph_device_per_frame_unregister(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle);
