#include "photon_device_internal.h"
#include "photon_instance_internal.h"
#include "photon/photon_error.h"
#include "photon/photon_log.h"
#include "photon/photon_status.h"
#include <vulkan/vulkan_core.h>
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"

static bool _has_extension(VkExtensionProperties *exts, uint32_t count, const char *name)
{
    for (uint32_t i = 0; i < count; i++)
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    return false;
}

static PhStatus _phys_device_meets_requirements(VkPhysicalDevice physDevice, PhCapabilityRequests caps, bool *pMeetsRequirements)
{
    PhStatus                 status        = PH_SUCCESS;
    VkQueueFamilyProperties *queueFamilies = NULL;
    VkExtensionProperties   *exts          = NULL;

    *pMeetsRequirements = false;

    VkPhysicalDeviceProperties props = { 0 };
    vkGetPhysicalDeviceProperties(physDevice, &props);

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &accelFeatures,
    };
    VkPhysicalDeviceVulkan13Features vk13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &rtFeatures,
    };
    VkPhysicalDeviceVulkan12Features vk12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vk13,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vk12,
    };
    vkGetPhysicalDeviceFeatures2(physDevice, &features2);

    VkPhysicalDeviceMemoryProperties memProps = { 0 };
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, NULL);
    queueFamilies = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    PH_CHECK_GOTO(PH_LOG_ERROR, queueFamilies, PH_ERR_OUT_OF_MEMORY, status, exit);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies);

    uint32_t extCount = 0;
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateDeviceExtensionProperties(physDevice, NULL, &extCount, NULL), status, exit);
    exts = malloc(sizeof(VkExtensionProperties) * extCount);
    PH_CHECK_GOTO(PH_LOG_ERROR, exts, PH_ERR_OUT_OF_MEMORY, status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateDeviceExtensionProperties(physDevice, NULL, &extCount, exts), status, exit);

    bool ok = true;

    if (caps.discrete && props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        ok = false;

    if (caps.rtCapable && (!rtFeatures.rayTracingPipeline || !accelFeatures.accelerationStructure
        || !_has_extension(exts, extCount, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
        || !_has_extension(exts, extCount, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
        || !_has_extension(exts, extCount, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)))
        ok = false;

    if (caps.swapchain && !_has_extension(exts, extCount, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        ok = false;

    if (caps.samplerAnisotropy    && !features2.features.samplerAnisotropy)    ok = false;
    if (caps.fillModeNonSolid     && !features2.features.fillModeNonSolid)     ok = false;
    if (caps.wideLines            && !features2.features.wideLines)            ok = false;
    if (caps.largePoints          && !features2.features.largePoints)          ok = false;
    if (caps.multiDrawIndirect    && !features2.features.multiDrawIndirect)    ok = false;
    if (caps.shaderInt64          && !features2.features.shaderInt64)          ok = false;
    if (caps.shaderFloat64        && !features2.features.shaderFloat64)        ok = false;

    if (caps.bufferDeviceAddress  && !vk12.bufferDeviceAddress)                ok = false;
    if (caps.descriptorIndexing   && (!vk12.descriptorIndexing
                                   || !vk12.runtimeDescriptorArray))           ok = false;
    if (caps.timelineSemaphore    && !vk12.timelineSemaphore)                  ok = false;

    if (caps.dynamicRendering     && !vk13.dynamicRendering)                   ok = false;
    if (caps.synchronization2     && !vk13.synchronization2)                   ok = false;

    if (caps.minimumImageDimensions.width  > 0
        && props.limits.maxImageDimension2D < caps.minimumImageDimensions.width)  ok = false;
    if (caps.minimumImageDimensions.height > 0
        && props.limits.maxImageDimension2D < caps.minimumImageDimensions.height) ok = false;
    if (caps.minPushConstantsSize > 0
        && props.limits.maxPushConstantsSize < caps.minPushConstantsSize)         ok = false;

    if (caps.minimumVRAM > 0)
    {
        uint64_t vram = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                vram += memProps.memoryHeaps[i].size;
        if (vram < caps.minimumVRAM)
            ok = false;
    }

    bool hasGraphics = false, hasAsyncCompute = false, hasDedicatedTransfer = false;
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        VkQueueFlags f = queueFamilies[i].queueFlags;
        if (f & VK_QUEUE_GRAPHICS_BIT)
            hasGraphics = true;
        if ((f & VK_QUEUE_COMPUTE_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT))
            hasAsyncCompute = true;
        if ((f & VK_QUEUE_TRANSFER_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT) && !(f & VK_QUEUE_COMPUTE_BIT))
            hasDedicatedTransfer = true;
    }
    if (caps.graphicsQueue     && !hasGraphics)          ok = false;
    if (caps.asyncComputeQueue && !hasAsyncCompute)      ok = false;
    if (caps.dedicatedTransfer && !hasDedicatedTransfer) ok = false;

    if (ok)
    {
        PH_LOG_INFO("Found acceptable physical device: %s", props.deviceName);
    }
    *pMeetsRequirements = ok;

exit:
    PH_FREE_IF_SET(queueFamilies);
    PH_FREE_IF_SET(exts);
    return status;
}

PhStatus ph_enumerate_devices(PhInstanceHandle hInstance, PhCapabilityRequests caps, PhDeviceInfoSpan *ppDeviceInfo)
{
    PhStatus status = PH_SUCCESS;
    uint32_t physDeviceCount = 0;
    VkPhysicalDevice *pPhysicalDevices = NULL;
    PhDeviceInfo *pDeviceInfos = NULL;
    uint32_t deviceInfoCount = 0;

    PH_NULL_CHECK(PH_LOG_ERROR, ppDeviceInfo);

    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumeratePhysicalDevices(hInstance->instance, &physDeviceCount, NULL), 
        status, exit);
    pPhysicalDevices = malloc(sizeof(VkPhysicalDevice) * physDeviceCount);
    PH_CHECK_GOTO(PH_LOG_ERROR, pPhysicalDevices != NULL, PH_ERR_OUT_OF_MEMORY, status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumeratePhysicalDevices(hInstance->instance, &physDeviceCount, pPhysicalDevices),
        status, exit);

    pDeviceInfos = malloc(sizeof(PhDeviceInfo) * physDeviceCount);
    PH_CHECK_GOTO(PH_LOG_ERROR, pDeviceInfos != NULL, PH_ERR_OUT_OF_MEMORY, status, exit);

    for (size_t i = 0; i < physDeviceCount; i++)
    {
        bool deviceMeetsRequirements = false;
        PH_PROPAGATE_GOTO(PH_LOG_ERROR, _phys_device_meets_requirements(pPhysicalDevices[i], caps, &deviceMeetsRequirements), status, exit);
        if (deviceMeetsRequirements)
        {
            PhDevice *pDevice = malloc(sizeof(PhDevice));
            PH_CHECK_GOTO(PH_LOG_ERROR, pDevice != NULL, PH_ERR_OUT_OF_MEMORY, status, exit);

            pDevice->physDevice = pPhysicalDevices[i];
            vkGetPhysicalDeviceProperties(pPhysicalDevices[i], &pDevice->props);
            vkGetPhysicalDeviceFeatures(pPhysicalDevices[i], &pDevice->features);
            
            pDeviceInfos[deviceInfoCount].handle = pDevice;
            deviceInfoCount++;
        }
    }

exit:
    PH_FREE_IF_SET(pPhysicalDevices);
    if (status == PH_SUCCESS)
    {
        ppDeviceInfo->len =  deviceInfoCount;
        ppDeviceInfo->ptr =  pDeviceInfos;
    }
    else
    {
        for (size_t i = 0; i < deviceInfoCount; i++)
        {
            PH_FREE_IF_SET(pDeviceInfos[i].handle);
        }
        PH_FREE_IF_SET(pDeviceInfos);
    }
    return status;
}