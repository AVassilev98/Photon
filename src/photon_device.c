#include "photon/photon_device.h"
#include "photon/photon_pipeline.h"
#include "photon/photon_window.h"
#include "photon_device_internal.h"
#include "photon_instance_internal.h"
#include "photon/photon_error.h"
#include "photon/photon_log.h"
#include "photon/photon_status.h"
#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>
#include "stdlib.h"
#include "stdbool.h"
#include "string.h"
#include "vk_mem_alloc.h"

static bool _has_extension(VkExtensionProperties *exts, uint32_t count, const char *name)
{
    for (uint32_t i = 0; i < count; i++)
        if (strcmp(exts[i].extensionName, name) == 0)
            return true;
    return false;
}

static PhStatus _phys_device_meets_requirements(VkPhysicalDevice physDevice, PhCapability caps, bool *pMeetsRequirements)
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
    PH_MALLOC_GOTO(PH_LOG_ERROR, queueFamilies, sizeof(VkQueueFamilyProperties) * queueFamilyCount, status, exit);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies);

    uint32_t extCount = 0;
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateDeviceExtensionProperties(physDevice, NULL, &extCount, NULL), status, exit);
    PH_MALLOC_GOTO(PH_LOG_ERROR, exts, sizeof(VkExtensionProperties) * extCount, status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateDeviceExtensionProperties(physDevice, NULL, &extCount, exts), status, exit);

#define _WARN_MISSING(cond, name) \
    do { if (cond) { PH_LOG_WARN("  [%s] missing: " name, props.deviceName); ok = false; } } while(0)

    bool ok = true;

    _WARN_MISSING(caps.discrete && props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        "discrete GPU");

    if (caps.rtCapable)
    {
        _WARN_MISSING(!rtFeatures.rayTracingPipeline,
            "rayTracingPipeline feature");
        _WARN_MISSING(!accelFeatures.accelerationStructure,
            "accelerationStructure feature");
        _WARN_MISSING(!_has_extension(exts, extCount, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME),
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        _WARN_MISSING(!_has_extension(exts, extCount, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME),
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        _WARN_MISSING(!_has_extension(exts, extCount, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME),
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    }

    _WARN_MISSING(caps.swapchain       && !_has_extension(exts, extCount, VK_KHR_SWAPCHAIN_EXTENSION_NAME),
        VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    _WARN_MISSING(caps.samplerAnisotropy && !features2.features.samplerAnisotropy, "samplerAnisotropy");
    _WARN_MISSING(caps.fillModeNonSolid  && !features2.features.fillModeNonSolid,  "fillModeNonSolid");
    _WARN_MISSING(caps.wideLines         && !features2.features.wideLines,         "wideLines");
    _WARN_MISSING(caps.largePoints       && !features2.features.largePoints,       "largePoints");
    _WARN_MISSING(caps.multiDrawIndirect && !features2.features.multiDrawIndirect, "multiDrawIndirect");
    _WARN_MISSING(caps.shaderInt64       && !features2.features.shaderInt64,       "shaderInt64");
    _WARN_MISSING(caps.shaderFloat64     && !features2.features.shaderFloat64,     "shaderFloat64");

    _WARN_MISSING(caps.bufferDeviceAddress && !vk12.bufferDeviceAddress,           "bufferDeviceAddress");
    _WARN_MISSING(caps.descriptorIndexing  && !vk12.descriptorIndexing,            "descriptorIndexing");
    _WARN_MISSING(caps.descriptorIndexing  && !vk12.runtimeDescriptorArray,        "runtimeDescriptorArray");
    _WARN_MISSING(caps.timelineSemaphore   && !vk12.timelineSemaphore,             "timelineSemaphore");

    _WARN_MISSING(caps.dynamicRendering  && !vk13.dynamicRendering,                "dynamicRendering");
    _WARN_MISSING(caps.synchronization2  && !vk13.synchronization2,               "synchronization2");

    _WARN_MISSING(caps.minimumImageDimensions.width  > 0
        && props.limits.maxImageDimension2D < caps.minimumImageDimensions.width,
        "minimumImageDimensions.width");
    _WARN_MISSING(caps.minimumImageDimensions.height > 0
        && props.limits.maxImageDimension2D < caps.minimumImageDimensions.height,
        "minimumImageDimensions.height");
    _WARN_MISSING(caps.minPushConstantsSize > 0
        && props.limits.maxPushConstantsSize < caps.minPushConstantsSize,
        "minPushConstantsSize");

    if (caps.minimumVRAM > 0)
    {
        uint64_t vram = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; i++)
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                vram += memProps.memoryHeaps[i].size;
        _WARN_MISSING(vram < caps.minimumVRAM, "minimumVRAM");
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
    _WARN_MISSING(caps.graphicsQueue     && !hasGraphics,          "graphics queue");
    _WARN_MISSING(caps.asyncComputeQueue && !hasAsyncCompute,      "async compute queue");
    _WARN_MISSING(caps.dedicatedTransfer && !hasDedicatedTransfer, "dedicated transfer queue");

#undef _WARN_MISSING

    *pMeetsRequirements = ok;

exit:
    PH_FREE_IF_SET(queueFamilies);
    PH_FREE_IF_SET(exts);
    return status;
}

static PhStatus _initialize_ph_device_info(VkPhysicalDevice physDevice, PhCapability caps, PhDeviceInfo *pDeviceInfo)
{
    PhStatus                 status        = PH_SUCCESS;
    VkQueueFamilyProperties *queueFamilies = NULL;
    const char             **extensions    = NULL;
    VkExtensionProperties   *availExts     = NULL;

    PH_NULL_CHECK(PH_LOG_ERROR, pDeviceInfo);
    *pDeviceInfo = (PhDeviceInfo){ 0 };
    pDeviceInfo->capabilities         = caps;
    
    PH_MALLOC_GOTO(PH_LOG_ERROR, pDeviceInfo->handle, sizeof(PhDevice), status, exit);
    memset(pDeviceInfo->handle, 0, sizeof(PhDevice));

    pDeviceInfo->handle->physDevice   = physDevice;
    vkGetPhysicalDeviceProperties(physDevice, &pDeviceInfo->handle->props);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, NULL);
    PH_MALLOC_GOTO(PH_LOG_ERROR, queueFamilies, sizeof(VkQueueFamilyProperties) * queueFamilyCount, status, exit);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies);
    pDeviceInfo->pName   = pDeviceInfo->handle->props.deviceName;

    uint32_t graphicsFamily  = UINT32_MAX;
    uint32_t computeFamily   = UINT32_MAX;
    uint32_t transferFamily  = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        VkQueueFlags f = queueFamilies[i].queueFlags;
        if (graphicsFamily == UINT32_MAX && (f & VK_QUEUE_GRAPHICS_BIT))
        {
            graphicsFamily = i;
            if (computeFamily == UINT32_MAX && (f & VK_QUEUE_COMPUTE_BIT))
            {
                computeFamily = i;
            }
            if (transferFamily == UINT32_MAX && (f & VK_QUEUE_TRANSFER_BIT))
            {
                transferFamily = i;
            }
        }
        if ((f & VK_QUEUE_COMPUTE_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT))
            computeFamily = i;
        if ((f & VK_QUEUE_TRANSFER_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT) && !(f & VK_QUEUE_COMPUTE_BIT))
            transferFamily = i;
    }

    /* Fall back to graphics queue if no dedicated compute/transfer family found */
    if (computeFamily  == UINT32_MAX) computeFamily  = graphicsFamily;
    if (transferFamily == UINT32_MAX) transferFamily = graphicsFamily;

    float                  queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfos[3];
    VkCommandPoolCreateInfo commandPoolInfos[3];
    uint32_t                queueInfoCount = 0;

    if (graphicsFamily != UINT32_MAX)
    {
        queueInfos[queueInfoCount] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = graphicsFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
        commandPoolInfos[queueInfoCount] = (VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = graphicsFamily,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        queueInfoCount++;
    }
    if (computeFamily != UINT32_MAX && computeFamily != graphicsFamily)
    {
        queueInfos[queueInfoCount] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = computeFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
        commandPoolInfos[queueInfoCount] = (VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = computeFamily,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        queueInfoCount++;
    }
    if (transferFamily != UINT32_MAX && transferFamily != graphicsFamily && transferFamily != computeFamily)
    {
        queueInfos[queueInfoCount] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = transferFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
        commandPoolInfos[queueInfoCount] = (VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .queueFamilyIndex = transferFamily,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        queueInfoCount++;
    }

    uint32_t availExtCount = 0;
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateDeviceExtensionProperties(physDevice, NULL, &availExtCount, NULL), status, exit);
    PH_MALLOC_GOTO(PH_LOG_ERROR, availExts, sizeof(VkExtensionProperties) * availExtCount, status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateDeviceExtensionProperties(physDevice, NULL, &availExtCount, availExts), status, exit);

    uint32_t extCount = 0;
    PH_MALLOC_GOTO(PH_LOG_ERROR, extensions, sizeof(const char *) * 8, status, exit);

    if (caps.swapchain)
        extensions[extCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (caps.rtCapable)
    {
        extensions[extCount++] = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;
        extensions[extCount++] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
        extensions[extCount++] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
    }
    if (_has_extension(availExts, availExtCount, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        extensions[extCount++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures = {
        .sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = caps.rtCapable,
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures = {
        .sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext              = caps.rtCapable ? &accelFeatures : NULL,
        .rayTracingPipeline = caps.rtCapable,
    };
    VkPhysicalDeviceVulkan13Features vk13 = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext            = caps.rtCapable ? &rtFeatures : NULL,
        .dynamicRendering = caps.dynamicRendering,
        .synchronization2 = caps.synchronization2,
    };
    VkPhysicalDeviceVulkan12Features vk12 = {
        .sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext                                        = &vk13,
        .bufferDeviceAddress                          = caps.bufferDeviceAddress,
        .descriptorIndexing                           = caps.descriptorIndexing,
        .runtimeDescriptorArray                       = caps.descriptorIndexing,
        .descriptorBindingPartiallyBound              = caps.descriptorIndexing,
        .descriptorBindingSampledImageUpdateAfterBind = caps.descriptorIndexing,
        .shaderSampledImageArrayNonUniformIndexing    = caps.descriptorIndexing,
        .timelineSemaphore                            = caps.timelineSemaphore,
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext    = &vk12,
        .features = {
            .samplerAnisotropy = caps.samplerAnisotropy,
            .fillModeNonSolid  = caps.fillModeNonSolid,
            .wideLines         = caps.wideLines,
            .largePoints       = caps.largePoints,
            .multiDrawIndirect = caps.multiDrawIndirect,
            .shaderInt64       = caps.shaderInt64,
            .shaderFloat64     = caps.shaderFloat64,
        },
    };
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &features2,
        .extendedDynamicState = caps.extendedDynamicState,
    };

    VkDeviceCreateInfo deviceInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &extendedDynamicStateFeatures,
        .queueCreateInfoCount    = queueInfoCount,
        .pQueueCreateInfos       = queueInfos,
        .enabledExtensionCount   = extCount,
        .ppEnabledExtensionNames = extensions,
    };

    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateDevice(physDevice, &deviceInfo, NULL, &pDeviceInfo->handle->device), status, exit);
    
    pDeviceInfo->handle->graphicsQueueFamily = graphicsFamily;
    pDeviceInfo->handle->computeQueueFamily = computeFamily;
    pDeviceInfo->handle->transferQueueFamily = transferFamily;

    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateCommandPool(pDeviceInfo->handle->device, &commandPoolInfos[0], NULL, &pDeviceInfo->handle->graphicsPool), status, exit);

    if (computeFamily != graphicsFamily)
    {
        PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateCommandPool(pDeviceInfo->handle->device, &commandPoolInfos[1], NULL, &pDeviceInfo->handle->computePool), status, exit);
    }
    else
    {
        pDeviceInfo->handle->computePool = pDeviceInfo->handle->graphicsPool;
    }

    if (transferFamily != graphicsFamily && transferFamily != computeFamily)
    {
        uint32_t transferPoolIdx = (computeFamily != graphicsFamily) ? 2 : 1;
        PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateCommandPool(pDeviceInfo->handle->device, &commandPoolInfos[transferPoolIdx], NULL, &pDeviceInfo->handle->transferPool), status, exit);
    }
    else
    {
        pDeviceInfo->handle->transferPool = (transferFamily == computeFamily)
            ? pDeviceInfo->handle->computePool
            : pDeviceInfo->handle->graphicsPool;
    }

    vkGetDeviceQueue(pDeviceInfo->handle->device, graphicsFamily,  0, &pDeviceInfo->handle->graphicsQueue);
    vkGetDeviceQueue(pDeviceInfo->handle->device, computeFamily,   0, &pDeviceInfo->handle->computeQueue);
    vkGetDeviceQueue(pDeviceInfo->handle->device, transferFamily,  0, &pDeviceInfo->handle->transferQueue);

    {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1024 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1024 },
            { VK_DESCRIPTOR_TYPE_SAMPLER,                 256 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           256 },
        };
        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = 4096,
            .poolSizeCount = PH_NUM_ELEMS(poolSizes),
            .pPoolSizes    = poolSizes,
        };
        PH_VK_CHECK_GOTO(PH_LOG_ERROR,
            vkCreateDescriptorPool(pDeviceInfo->handle->device, &poolInfo, NULL, &pDeviceInfo->handle->descriptorPool),
            status, exit);
    }

exit:
    if (status != PH_SUCCESS)
    {
        PH_FREE_IF_SET(pDeviceInfo->handle);
    }
    PH_FREE_IF_SET(queueFamilies);
    PH_FREE_IF_SET(extensions);
    PH_FREE_IF_SET(availExts);
    return status;
}

PhStatus ph_devices_enumerate(PhInstanceHandle hInstance, PhCapability caps, PhDeviceInfoSpan *pDeviceInfoSpan)
{
    PhStatus status = PH_SUCCESS;
    uint32_t physDeviceCount = 0;
    VkPhysicalDevice *pPhysicalDevices = NULL;
    PhDeviceInfo *pDeviceInfos = NULL;
    uint32_t deviceInfoCount = 0;

    PH_NULL_CHECK(PH_LOG_ERROR, pDeviceInfoSpan);

    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumeratePhysicalDevices(hInstance->instance, &physDeviceCount, NULL), 
        status, exit);
    PH_MALLOC_GOTO(PH_LOG_ERROR, pPhysicalDevices, sizeof(VkPhysicalDevice) * physDeviceCount, status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumeratePhysicalDevices(hInstance->instance, &physDeviceCount, pPhysicalDevices),
        status, exit);

    PH_MALLOC_GOTO(PH_LOG_ERROR, pDeviceInfos, sizeof(PhDeviceInfo) * physDeviceCount, status, exit);
    memset(pDeviceInfos, 0, sizeof(PhDeviceInfo) * physDeviceCount);

    for (size_t i = 0; i < physDeviceCount; i++)
    {
        bool deviceMeetsRequirements = false;
        PH_PROPAGATE_GOTO(PH_LOG_ERROR, _phys_device_meets_requirements(pPhysicalDevices[i], caps, &deviceMeetsRequirements), status, exit);
        if (deviceMeetsRequirements)
        {
            PH_PROPAGATE_GOTO(PH_LOG_ERROR, _initialize_ph_device_info(pPhysicalDevices[i], caps, &pDeviceInfos[deviceInfoCount]), status, exit);
            PH_LOG_INFO("Initialized device: %s", pDeviceInfos[deviceInfoCount].pName);

            PhDeviceHandle hDevice = pDeviceInfos[deviceInfoCount].handle;
            hDevice->instance = hInstance->instance;
            VmaAllocatorCreateInfo allocatorInfo = {
                .physicalDevice   = pPhysicalDevices[i],
                .device           = hDevice->device,
                .instance         = hInstance->instance,
                .vulkanApiVersion = VK_API_VERSION_1_3,
            };
            PH_VK_CHECK_GOTO(PH_LOG_ERROR, vmaCreateAllocator(&allocatorInfo, &hDevice->allocator), status, exit);
            PhPerFrameResourceVec_init(&hDevice->perFrameResources);

            deviceInfoCount++;
        }
    }
    PH_CHECK_GOTO(PH_LOG_ERROR, deviceInfoCount > 0, PH_ERR_NOT_FOUND, status, exit);

exit:
    PH_FREE_IF_SET(pPhysicalDevices);
    if (status == PH_SUCCESS)
    {
        pDeviceInfoSpan->len =  deviceInfoCount;
        pDeviceInfoSpan->ptr =  pDeviceInfos;
    }
    else
    {
        for (size_t i = 0; i < deviceInfoCount; i++)
        {
            if (pDeviceInfos[i].handle->allocator)
                vmaDestroyAllocator(pDeviceInfos[i].handle->allocator);
            PH_FREE_IF_SET(pDeviceInfos[i].handle);
        }
        PH_FREE_IF_SET(pDeviceInfos);
    }
    return status;
}

static bool _ph_requested_surface_format_in_list(VkSurfaceFormatKHR desiredFormat, VkSurfaceFormatKHR *pSurfaceFormats, uint32_t surfaceFormatCount)
{
    for (size_t i = 0; i < surfaceFormatCount; i++)
    {
        if (desiredFormat.colorSpace == pSurfaceFormats[i].colorSpace &&
            desiredFormat.format == pSurfaceFormats[i].format)
        {
            return true;
        }
    }
    return false;
}

static bool _ph_requested_present_mode_in_list(VkPresentModeKHR desiredMode, VkPresentModeKHR *pPresentModes, uint32_t presentModeCount)
{
    for (size_t i = 0; i < presentModeCount; i++)
    {
        if (desiredMode == pPresentModes[i])
        {
            return true;
        }
    }
    return false;
}


static void _ph_swapchain_cleanup(PhDeviceHandle hDevice)
{
    for (uint32_t i = 0; i < hDevice->swapchainImageCount; i++)
    {
        vkDestroyImageView(hDevice->device, hDevice->pSwapchainImageViews[i], NULL);
        vkDestroySemaphore(hDevice->device, hDevice->pPresentSemaphores[i], NULL);
        vkDestroySemaphore(hDevice->device, hDevice->pRenderSemaphores[i], NULL);
        vkDestroyFence(hDevice->device, hDevice->pPresentFences[i], NULL);
    }
    PH_FREE_IF_SET(hDevice->pSwapchainImages);
    PH_FREE_IF_SET(hDevice->pSwapchainImageViews);
    PH_FREE_IF_SET(hDevice->pPresentSemaphores);
    PH_FREE_IF_SET(hDevice->pRenderSemaphores);
    PH_FREE_IF_SET(hDevice->pPresentFences);

    hDevice->pSwapchainImages     = NULL;
    hDevice->pSwapchainImageViews = NULL;
    hDevice->pPresentSemaphores   = NULL;
    hDevice->pRenderSemaphores    = NULL;
    hDevice->pPresentFences       = NULL;
    hDevice->swapchainImageCount  = 0;
}

static PhStatus _ph_swapchain_create_resources(PhDeviceHandle hDevice)
{
    PH_VK_CHECK(PH_LOG_ERROR,
        vkGetSwapchainImagesKHR(hDevice->device, hDevice->swapchain, &hDevice->swapchainImageCount, NULL));
    PH_MALLOC(PH_LOG_ERROR, hDevice->pSwapchainImages, sizeof(VkImage) * hDevice->swapchainImageCount);
    PH_VK_CHECK(PH_LOG_ERROR,
        vkGetSwapchainImagesKHR(hDevice->device, hDevice->swapchain, &hDevice->swapchainImageCount, hDevice->pSwapchainImages));

    PH_MALLOC(PH_LOG_ERROR, hDevice->pSwapchainImageViews, sizeof(VkImageView) * hDevice->swapchainImageCount);
    PH_MALLOC(PH_LOG_ERROR, hDevice->pPresentSemaphores, sizeof(VkSemaphore) * hDevice->swapchainImageCount);
    PH_MALLOC(PH_LOG_ERROR, hDevice->pRenderSemaphores, sizeof(VkSemaphore) * hDevice->swapchainImageCount);
    PH_MALLOC(PH_LOG_ERROR, hDevice->pPresentFences, sizeof(VkFence) * hDevice->swapchainImageCount);

    for (uint32_t i = 0; i < hDevice->swapchainImageCount; i++)
    {
        VkImageViewCreateInfo viewInfo = {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = hDevice->pSwapchainImages[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = hDevice->swapchainFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        PH_VK_CHECK(PH_LOG_ERROR,
            vkCreateImageView(hDevice->device, &viewInfo, NULL, &hDevice->pSwapchainImageViews[i]));

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        PH_VK_CHECK(PH_LOG_ERROR,
            vkCreateSemaphore(hDevice->device, &semaphoreCreateInfo, NULL, &hDevice->pPresentSemaphores[i]));
        PH_VK_CHECK(PH_LOG_ERROR,
            vkCreateSemaphore(hDevice->device, &semaphoreCreateInfo, NULL, &hDevice->pRenderSemaphores[i]));
        PH_VK_CHECK(PH_LOG_ERROR,
            vkCreateFence(hDevice->device, &fenceCreateInfo, NULL, &hDevice->pPresentFences[i]));
    }

    return PH_SUCCESS;
}

static PhStatus _ph_per_frame_resources_recreate(PhDeviceHandle hDevice, PhExtent2D newExtent)
{
    size_t count = PhPerFrameResourceVec_len(&hDevice->perFrameResources);
    for (size_t i = 0; i < count; i++)
    {
        PhPerFrameResourceInternal *internal = PhPerFrameResourceVec_get(&hDevice->perFrameResources, i);
        if (internal && internal->created && internal->recreate)
        {
            for (int f = 0; f < PH_MAX_FRAMES_IN_FLIGHT; f++)
            {
                void *elem = (uint8_t *)internal->data + f * internal->elemSize;
                PH_CHECK(PH_LOG_ERROR,
                    internal->recreate(hDevice, internal->recreateData, elem, newExtent));
            }
        }
    }
    return PH_SUCCESS;
}

PhStatus ph_device_configure_for_present(PhDeviceHandle hDevice, PhSurfaceHandle hSurface, PhPresentOptions opts)
{
    PhStatus status = PH_SUCCESS;
    uint32_t surfaceFormatCount = 0;
    VkSurfaceFormatKHR *pSurfaceFormats = NULL;
    uint32_t presentModeCount = 0;
    VkPresentModeKHR *pPresentModes = NULL;

    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_NULL_CHECK(PH_LOG_ERROR, hSurface);

    VkSurfaceCapabilitiesKHR surfaceCapabilities = { 0 };
    PH_VK_CHECK(PH_LOG_ERROR, 
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(hDevice->physDevice, hSurface, &surfaceCapabilities));

    PH_VK_CHECK(PH_LOG_ERROR, 
        vkGetPhysicalDeviceSurfaceFormatsKHR(hDevice->physDevice, hSurface, &surfaceFormatCount, NULL));
    PH_MALLOC_GOTO(PH_LOG_ERROR, pSurfaceFormats, sizeof(VkSurfaceFormatKHR) * surfaceFormatCount, status, cleanup);
    PH_VK_CHECK(PH_LOG_ERROR, 
        vkGetPhysicalDeviceSurfaceFormatsKHR(hDevice->physDevice, hSurface, &surfaceFormatCount, pSurfaceFormats));
    
    PH_VK_CHECK(PH_LOG_ERROR, 
        vkGetPhysicalDeviceSurfacePresentModesKHR(hDevice->physDevice, hSurface, &presentModeCount, NULL));
    PH_MALLOC_GOTO(PH_LOG_ERROR, pPresentModes, sizeof(VkPresentModeKHR) * presentModeCount, status, cleanup);
    PH_VK_CHECK(PH_LOG_ERROR, 
        vkGetPhysicalDeviceSurfacePresentModesKHR(hDevice->physDevice, hSurface, &presentModeCount, pPresentModes));

    PH_CHECK_GOTO(PH_LOG_ERROR, _ph_requested_surface_format_in_list(opts.format, pSurfaceFormats, surfaceFormatCount),
        PH_ERR_NOT_FOUND, status, cleanup);
    PH_CHECK_GOTO(PH_LOG_ERROR, _ph_requested_present_mode_in_list(opts.mode, pPresentModes, presentModeCount),
        PH_ERR_NOT_FOUND, status, cleanup);

    uint32_t presentFamily = UINT32_MAX;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(hDevice->physDevice, &queueFamilyCount, NULL);
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        VkBool32 supported = VK_FALSE;
        PH_VK_CHECK_GOTO(PH_LOG_ERROR,
            vkGetPhysicalDeviceSurfaceSupportKHR(hDevice->physDevice, i, hSurface, &supported),
            status, cleanup);
        if (supported)
        {
            presentFamily = i;
            break;
        }
    }
    PH_CHECK_GOTO(PH_LOG_ERROR, presentFamily != UINT32_MAX, PH_ERR_UNSUPPORTED, status, cleanup);

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
        imageCount = surfaceCapabilities.maxImageCount;

    VkExtent2D extent = surfaceCapabilities.currentExtent;

    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = hSurface,
        .minImageCount    = imageCount,
        .imageFormat      = opts.format.format,
        .imageColorSpace  = opts.format.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = surfaceCapabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = opts.mode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = VK_NULL_HANDLE,
    };

    PH_VK_CHECK_GOTO(PH_LOG_ERROR,
        vkCreateSwapchainKHR(hDevice->device, &swapchainInfo, NULL, &hDevice->swapchain),
        status, cleanup);

    hDevice->swapchainFormat = opts.format.format;
    hDevice->swapchainExtent = extent;
    hDevice->surface         = hSurface;
    hDevice->presentOptions  = opts;

    PH_PROPAGATE_GOTO(PH_LOG_ERROR, _ph_swapchain_create_resources(hDevice), status, cleanup);

cleanup:
    PH_FREE_IF_SET(pSurfaceFormats);
    PH_FREE_IF_SET(pPresentModes);
    return status;
}

PhStatus ph_device_swapchain_recreate(PhDeviceHandle hDevice)
{
    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_VK_CHECK(PH_LOG_ERROR, vkDeviceWaitIdle(hDevice->device));

    VkSwapchainKHR oldSwapchain = hDevice->swapchain;
    _ph_swapchain_cleanup(hDevice);

    VkSurfaceCapabilitiesKHR surfaceCaps = { 0 };
    PH_VK_CHECK(PH_LOG_ERROR,
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(hDevice->physDevice, hDevice->surface, &surfaceCaps));

    VkExtent2D extent = surfaceCaps.currentExtent;
    if (extent.width == 0 || extent.height == 0)
    {
        vkDestroySwapchainKHR(hDevice->device, oldSwapchain, NULL);
        hDevice->swapchain       = VK_NULL_HANDLE;
        hDevice->swapchainExtent = extent;
        return PH_SUCCESS;
    }

    uint32_t imageCount = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount)
        imageCount = surfaceCaps.maxImageCount;

    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = hDevice->surface,
        .minImageCount    = imageCount,
        .imageFormat      = hDevice->presentOptions.format.format,
        .imageColorSpace  = hDevice->presentOptions.format.colorSpace,
        .imageExtent      = extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = surfaceCaps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = hDevice->presentOptions.mode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = oldSwapchain,
    };

    PH_VK_CHECK(PH_LOG_ERROR,
        vkCreateSwapchainKHR(hDevice->device, &swapchainInfo, NULL, &hDevice->swapchain));
    vkDestroySwapchainKHR(hDevice->device, oldSwapchain, NULL);

    hDevice->swapchainExtent = extent;
    hDevice->frame = 0;

    PH_CHECK(PH_LOG_ERROR, _ph_swapchain_create_resources(hDevice));

    PhExtent2D newExtent = { .width = extent.width, .height = extent.height };
    PH_CHECK(PH_LOG_ERROR, _ph_per_frame_resources_recreate(hDevice, newExtent));

    return PH_SUCCESS;
}

PhStatus ph_device_extent_get(PhDeviceHandle hDevice, PhExtent2D *pExtent)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pExtent);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, hDevice->swapchain != VK_NULL_HANDLE, PH_ERR_NOT_FOUND);

    *pExtent = (PhExtent2D) {
        .width = hDevice->swapchainExtent.width,
        .height = hDevice->swapchainExtent.height,
    };
    return PH_SUCCESS;
}

PhStatus ph_device_frame_index_get(PhDeviceHandle hDevice, size_t *pIndex)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pIndex);
    *pIndex = hDevice->frame;

    return PH_SUCCESS;
}

PhStatus ph_device_create_staging_buffer(PhDeviceHandle hDevice, uint32_t size)
{
    PhBuffer stagingBuffer;

    if (size % PH_ALLOCATION_GRANULARITY)
    {
        PH_LOG_ERROR("Staging buffer must be a multiple of %d\n", PH_ALLOCATION_GRANULARITY / KB);
        return PH_ERR_INVALID_ARG;
    }
    
    PH_CHECK(PH_LOG_ERROR,
        ph_device_buffer_create(hDevice, PH_QUEUE_TYPE_TRANSFER_BIT, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, &stagingBuffer, 
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT));
    
    PH_CHECK(PH_LOG_ERROR, ph_device_buffer_map(hDevice, &stagingBuffer));

    hDevice->stagingBuffer = stagingBuffer;

    StagingBufferFreeBitVec_init(&hDevice->stagingBufferAllocations);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, 
        StagingBufferFreeBitVec_resize(&hDevice->stagingBufferAllocations, size / PH_ALLOCATION_GRANULARITY), PH_ERR_OUT_OF_MEMORY);
    PhTransferVector_init(&hDevice->activeTransfers);

    return PH_SUCCESS;
}

PhStatus ph_device_command_buffer_create(PhDeviceHandle hDevice, PhQueueType type, size_t count, PhCommandBuffer *pBuffers)
{
    VkCommandPool commandPool = { 0 };
    VkCommandBufferAllocateInfo bufferAllocateInfo = { 0 };

    PH_NULL_CHECK(PH_LOG_ERROR, pBuffers);
    
    switch (type)
    {
        case PH_QUEUE_TYPE_GRAPHICS_BIT:
            commandPool = hDevice->graphicsPool;
            break;
        case PH_QUEUE_TYPE_COMPUTE_BIT:
            commandPool = hDevice->computePool;
            break;
        case PH_QUEUE_TYPE_TRANSFER_BIT:
            commandPool = hDevice->transferPool;
            break;
        default:
            PH_CHECK_OR_RETURN(PH_LOG_ERROR, false, PH_ERR_INVALID_ARG);
    };

    bufferAllocateInfo = (VkCommandBufferAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandBufferCount = count,
        .commandPool = commandPool,
    };

    PH_VK_CHECK(PH_LOG_ERROR, vkAllocateCommandBuffers(hDevice->device, &bufferAllocateInfo, pBuffers));
    return PH_SUCCESS;
}

PhStatus ph_device_semaphore_create(PhDeviceHandle hDevice, PhSemaphore *out)
{
    const static VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    PH_VK_CHECK(PH_LOG_ERROR, vkCreateSemaphore(hDevice->device, &semaphoreCreateInfo, NULL, out));
    return PH_SUCCESS;
}
PhStatus ph_device_semaphore_destroy(PhDeviceHandle hDevice, PhSemaphore sem)
{
    vkDestroySemaphore(hDevice->device, sem, NULL);
    return PH_SUCCESS;
}

PhStatus ph_device_command_buffer_destroy(PhDeviceHandle hDevice, PhQueueType type, size_t count, PhCommandBuffer *pBuffers)
{
    VkCommandPool commandPool = { 0 };
    VkCommandBufferAllocateInfo bufferAllocateInfo = { 0 };
    
    switch (type)
    {
        case PH_QUEUE_TYPE_GRAPHICS_BIT:
            commandPool = hDevice->graphicsPool;
            break;
        case PH_QUEUE_TYPE_COMPUTE_BIT:
            commandPool = hDevice->computePool;
            break;
        case PH_QUEUE_TYPE_TRANSFER_BIT:
            commandPool = hDevice->transferPool;
            break;
        default:
            PH_CHECK_OR_RETURN(PH_LOG_ERROR, false, PH_ERR_INVALID_ARG);
    };

    vkFreeCommandBuffers(hDevice->device, commandPool, count, pBuffers);
    return PH_SUCCESS;
}

PhStatus ph_device_present_image_get_next(PhDeviceHandle hDevice, PhImage *image)
{
    size_t currentImageIndex = hDevice->frame % hDevice->swapchainImageCount;
    uint32_t imageIndex = 0;
    VkResult acquireResult = VK_SUCCESS;

    PH_NULL_CHECK(PH_LOG_ERROR, image);

    PH_VK_CHECK(PH_LOG_ERROR,
        vkWaitForFences(hDevice->device, 1, &hDevice->pPresentFences[currentImageIndex], VK_TRUE, UINT64_MAX));
    PH_VK_CHECK(PH_LOG_ERROR,
        vkResetFences(hDevice->device, 1, &hDevice->pPresentFences[currentImageIndex]));

    acquireResult = vkAcquireNextImageKHR(hDevice->device, hDevice->swapchain, UINT64_MAX,
        hDevice->pPresentSemaphores[currentImageIndex], VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
    {
        PH_LOG_INFO("Acquire image returned %s", 
            acquireResult == VK_ERROR_OUT_OF_DATE_KHR ? "VK_ERROR_OUT_OF_DATE_KHR" : "VK_SUBOPTIMAL_KHR");
    }

    *image = (PhImage) {
        .image = hDevice->pSwapchainImages[imageIndex],
        .defaultView = hDevice->pSwapchainImageViews[imageIndex],
        .extent = hDevice->swapchainExtent,
        .readySemaphore = hDevice->pPresentSemaphores[imageIndex],
    };

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) { return PH_ERR_SWAPCHAIN_OUT_OF_DATE; }
    if (acquireResult == VK_SUBOPTIMAL_KHR) { return PH_ERR_SWAPCHAIN_SUBOPTIMAL; }
    return PH_SUCCESS;
}

PhStatus ph_device_queue_submit(PhDeviceHandle hDevice, PhQueueType type, PhQueueSubmitInfo *submitInfo)
{
    VkQueue queue = NULL;
    switch (type)
    {
        case PH_QUEUE_TYPE_GRAPHICS_BIT:
            queue = hDevice->graphicsQueue;
            break;
        case PH_QUEUE_TYPE_TRANSFER_BIT:
            queue = hDevice->transferQueue;
            break;
        case PH_QUEUE_TYPE_COMPUTE_BIT:
            queue = hDevice->computeQueue;
            break;
        default:
            PH_CHECK_OR_RETURN(PH_LOG_ERROR, false, PH_ERR_INVALID_ARG);

    }

    PH_VK_CHECK(PH_LOG_ERROR, vkQueueSubmit(queue, 1UL, submitInfo, NULL));
    return PH_SUCCESS;
}

PhStatus ph_device_present(PhDeviceHandle hDevice, PhSemaphore *pWaitSemaphores, size_t numSemaphores)
{
    VkResult presentStatus = VK_SUCCESS;
    uint32_t currentImageIndex = hDevice->frame % hDevice->swapchainImageCount;
    PhCommandBuffer buffer;

    PH_CHECK(PH_LOG_ERROR, ph_device_command_buffer_create(hDevice, PH_QUEUE_TYPE_GRAPHICS_BIT, 1, &buffer));
    
    VkCommandBufferBeginInfo bufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    PH_VK_CHECK(PH_LOG_ERROR, vkBeginCommandBuffer(buffer, &bufferBeginInfo));
    VkImageMemoryBarrier toPresent = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = 0,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = hDevice->pSwapchainImages[currentImageIndex],
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    vkCmdPipelineBarrier(buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, NULL, 0, NULL, 1, &toPresent);

    PH_VK_CHECK(PH_LOG_ERROR, vkEndCommandBuffer(buffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = NULL,
        .waitSemaphoreCount   = numSemaphores,
        .pWaitSemaphores      = pWaitSemaphores,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &hDevice->pRenderSemaphores[currentImageIndex],
    };
    
    PH_VK_CHECK(PH_LOG_ERROR,
        vkQueueSubmit(hDevice->graphicsQueue, 1, &submitInfo, hDevice->pPresentFences[currentImageIndex]));

    VkPresentInfoKHR presentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &hDevice->pRenderSemaphores[currentImageIndex],
        .swapchainCount     = 1,
        .pSwapchains        = &hDevice->swapchain,
        .pImageIndices      = &currentImageIndex,
    };
    presentStatus = vkQueuePresentKHR(hDevice->graphicsQueue, &presentInfo);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, presentStatus == VK_SUCCESS || presentStatus == VK_SUBOPTIMAL_KHR, PH_ERR_VK);
    if (presentStatus == VK_ERROR_OUT_OF_DATE_KHR || presentStatus == VK_SUBOPTIMAL_KHR)
    {
        PH_LOG_INFO("Present image returned %s", 
            presentStatus == VK_ERROR_OUT_OF_DATE_KHR ? "VK_ERROR_OUT_OF_DATE_KHR" : "VK_SUBOPTIMAL_KHR");
    }

    hDevice->frame++;
    return PH_SUCCESS;
}

PhStatus ph_device_buffer_create(PhDeviceHandle hDevice, PhQueueType queueTypeFlags, uint32_t size, PhBufferUsageFlags flags, PhSharingMode sharing, PhBuffer *out, VmaAllocationCreateFlags vmaFlags)
{
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, size > 0, PH_ERR_INVALID_ARG);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    VmaAllocation allocation;
    VkBuffer buffer;

    uint32_t numQueueFamilies = 0;
    uint32_t queueFamilyIndices[PH_NUM_QUEUE_TYPES] = {};
    if (queueTypeFlags & PH_QUEUE_TYPE_GRAPHICS_BIT)
    {
        queueFamilyIndices[numQueueFamilies++] = hDevice->graphicsQueueFamily;
    }
    if (queueTypeFlags & PH_QUEUE_TYPE_COMPUTE_BIT)
    {
        queueFamilyIndices[numQueueFamilies++] = hDevice->computeQueueFamily;
    }
    if (queueTypeFlags & PH_QUEUE_TYPE_TRANSFER_BIT)
    {
        queueFamilyIndices[numQueueFamilies++] = hDevice->transferQueueFamily;
    }

    VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndexCount = numQueueFamilies,
        .pQueueFamilyIndices = queueFamilyIndices,
        .size = size,
        .sharingMode = sharing,
        .usage = flags
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .flags = vmaFlags,
    };

    PH_VK_CHECK(PH_LOG_ERROR, 
        vmaCreateBuffer(hDevice->allocator, &bufferCreateInfo, &allocInfo, &buffer, &allocation, NULL));

    *out = (PhBuffer) {
        .buffer = buffer,
        .allocation = allocation,
        .flags = vmaFlags,
        .hostPtr = NULL,
        .size = size,
    };
    
    return PH_SUCCESS;
}

PhStatus ph_device_buffer_destroy(PhDeviceHandle hDevice, PhBuffer *buffer)
{    
    PH_NULL_CHECK(PH_LOG_ERROR, buffer);

    vmaDestroyBuffer(hDevice->allocator, buffer->buffer, buffer->allocation);
    return PH_SUCCESS;
}

static PhStatus _ph_device_staging_buffer_reclaim(PhDeviceHandle hDevice)
{
    uint32_t freeSlot = 0;
    uint32_t i = 0;
    while (i < hDevice->activeTransfers.len)
    {
        PhTransfer *transfer = &hDevice->activeTransfers.data[i];
        if (vkGetFenceStatus(hDevice->device, transfer->transferComplete) == VK_SUCCESS)
        {
            StagingBufferFreeBitVec_unset_range(&hDevice->stagingBufferAllocations, transfer->startChunk, transfer->numChunks);
            PhTransferVector_swap_remove(&hDevice->activeTransfers, i);
        }
        else
        {
            i++;
        }
    }

    return PH_SUCCESS;
}

PhStatus ph_device_buffer_upload(PhDeviceHandle hDevice, void *cpuData, uint32_t size, PhBuffer dest)
{
    PhStatus        status     = PH_SUCCESS;
    VkCommandBuffer cmd        = VK_NULL_HANDLE;
    VkFence         fence      = VK_NULL_HANDLE;
    uint32_t        startChunk = 0;
    uint32_t        idx        = 0;

    PH_NULL_CHECK(PH_LOG_ERROR, cpuData);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, size > 0, PH_ERR_INVALID_ARG);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, hDevice->stagingBuffer.hostPtr != NULL, PH_ERR_INVALID_STATE);

    uint32_t numChunks   = (size + PH_ALLOCATION_GRANULARITY - 1) / PH_ALLOCATION_GRANULARITY;
    size_t   totalChunks = StagingBufferFreeBitVec_len(&hDevice->stagingBufferAllocations);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, numChunks <= totalChunks, PH_ERR_INVALID_ARG);

    while (startChunk + numChunks <= totalChunks &&
           !StagingBufferFreeBitVec_all_clear(&hDevice->stagingBufferAllocations, startChunk, numChunks, &idx))
    {
        startChunk = idx + 1;
    }
    // If we have run out of memory, try to reclaim, then run again
    PH_CHECK(PH_LOG_ERROR, _ph_device_staging_buffer_reclaim(hDevice));
    while (startChunk + numChunks <= totalChunks &&
           !StagingBufferFreeBitVec_all_clear(&hDevice->stagingBufferAllocations, startChunk, numChunks, &idx))
    {
        startChunk = idx + 1;
    }

    // Error if we still can't find space after reclaiming
    PH_CHECK_GOTO(PH_LOG_ERROR, startChunk + numChunks <= totalChunks, PH_ERR_OUT_OF_MEMORY, status, exit);

    StagingBufferFreeBitVec_set_range(&hDevice->stagingBufferAllocations, startChunk, numChunks);
    memcpy((uint8_t *)hDevice->stagingBuffer.hostPtr + (size_t)startChunk * PH_ALLOCATION_GRANULARITY,
           cpuData, size);
    vmaFlushAllocation(hDevice->allocator, hDevice->stagingBuffer.allocation,
                       (VkDeviceSize)startChunk * PH_ALLOCATION_GRANULARITY, size);

    PH_PROPAGATE_GOTO(PH_LOG_ERROR,
        ph_device_command_buffer_create(hDevice, PH_QUEUE_TYPE_TRANSFER_BIT, 1, &cmd),
        status, exit);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, 
        vkBeginCommandBuffer(cmd, &beginInfo), status, exit);

    VkBufferCopy region = {
        .srcOffset = (VkDeviceSize)startChunk * PH_ALLOCATION_GRANULARITY,
        .dstOffset = 0,
        .size      = size,
    };
    vkCmdCopyBuffer(cmd, hDevice->stagingBuffer.buffer, dest.buffer, 1, &region);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, 
        vkEndCommandBuffer(cmd), status, exit);

    VkFenceCreateInfo fenceInfo = { 
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    };
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, 
        vkCreateFence(hDevice->device, &fenceInfo, NULL, &fence), status, exit);

    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, 
        vkQueueSubmit(hDevice->transferQueue, 1, &submitInfo, fence), status, exit);

    PH_CHECK_GOTO(PH_LOG_ERROR,
        PhTransferVector_push(&hDevice->activeTransfers,
            ((PhTransfer){ 
                .transferComplete = fence, 
                .startChunk = startChunk, 
                .numChunks = numChunks,
                .commandBuffer = cmd,
            })),
        PH_ERR_OUT_OF_MEMORY, status, exit);

    return PH_SUCCESS;

exit:
    if (fence      != VK_NULL_HANDLE) vkDestroyFence(hDevice->device, fence, NULL);
    if (cmd        != VK_NULL_HANDLE) ph_device_command_buffer_destroy(hDevice, PH_QUEUE_TYPE_TRANSFER_BIT, 1, &cmd);
    if (startChunk != UINT32_MAX)     StagingBufferFreeBitVec_unset_range(&hDevice->stagingBufferAllocations, startChunk, numChunks);
    return status;
}

PhStatus ph_device_buffer_map(PhDeviceHandle hDevice, PhBuffer *buffer)
{
    PH_NULL_CHECK(PH_LOG_ERROR, buffer);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, buffer->flags & VMA_ALLOCATION_CREATE_MAPPED_BIT, PH_ERR_INVALID_ARG);

    PH_VK_CHECK(PH_LOG_ERROR, vmaMapMemory(hDevice->allocator, buffer->allocation, &buffer->hostPtr));

    return PH_SUCCESS;
}

PhStatus ph_device_descriptor_sets_allocate(PhDeviceHandle hDevice,
                                            const VkDescriptorSetLayout *pLayouts, uint32_t count,
                                            PhDescriptorSet *pOut)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pLayouts);
    PH_NULL_CHECK(PH_LOG_ERROR, pOut);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, count > 0, PH_ERR_INVALID_ARG);

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = hDevice->descriptorPool,
        .descriptorSetCount = count,
        .pSetLayouts        = pLayouts,
    };

    PH_VK_CHECK(PH_LOG_ERROR, vkAllocateDescriptorSets(hDevice->device, &allocInfo, pOut));
    return PH_SUCCESS;
}

PhStatus ph_device_descriptor_sets_free(PhDeviceHandle hDevice,
                                        PhDescriptorSet *pSets, uint32_t count)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pSets);
    PH_VK_CHECK(PH_LOG_ERROR, vkFreeDescriptorSets(hDevice->device, hDevice->descriptorPool, count, pSets));
    return PH_SUCCESS;
}

PhStatus ph_device_descriptor_sets_write(PhDeviceHandle hDevice,
                                     const PhDescriptorWrite *pWrites, uint32_t writeCount)
{
    VkWriteDescriptorSet vkWrites[32];
    uint32_t remaining = writeCount;
    const PhDescriptorWrite *src = pWrites;

    while (remaining > 0)
    {
        uint32_t batch = remaining > 32 ? 32 : remaining;
        for (uint32_t i = 0; i < batch; i++)
        {
            vkWrites[i] = (VkWriteDescriptorSet){
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = src[i].set,
                .dstBinding      = src[i].binding,
                .dstArrayElement = src[i].arrayElement,
                .descriptorType  = src[i].type,
                .descriptorCount = src[i].count ? src[i].count : 1,
                .pBufferInfo     = src[i].pBufferInfo,
                .pImageInfo      = src[i].pImageInfo,
            };
        }
        vkUpdateDescriptorSets(hDevice->device, batch, vkWrites, 0, NULL);
        src       += batch;
        remaining -= batch;
    }

    return PH_SUCCESS;
}

PhStatus ph_device_per_frame_register(PhDeviceHandle hDevice, size_t elemSize, PhPerFrameCreateFn create, PhPerFrameDestroyFn destroy, PhPerFrameRecreateFn recreate, PhPerFrameResourceHandle *pHandle)
{
    PH_NULL_CHECK(PH_LOG_ERROR, create);
    PH_NULL_CHECK(PH_LOG_ERROR, destroy);
    PH_NULL_CHECK(PH_LOG_ERROR, pHandle);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, elemSize > 0, PH_ERR_INVALID_ARG);

    PhPerFrameResourceInternal internal = {
        .data         = NULL,
        .elemSize     = elemSize,
        .create       = create,
        .destroy      = destroy,
        .recreate     = recreate,
        .recreateData = NULL,
        .created      = false,
    };

    PH_CHECK_OR_RETURN(PH_LOG_ERROR,
        PhPerFrameResourceVec_push(&hDevice->perFrameResources, internal),
        PH_ERR_OUT_OF_MEMORY);

    *pHandle = (uint32_t)(PhPerFrameResourceVec_len(&hDevice->perFrameResources) - 1);
    return PH_SUCCESS;
}

PhStatus ph_device_per_frame_create(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle, void *pCreateParams)
{
    PhPerFrameResourceInternal *internal = PhPerFrameResourceVec_get(&hDevice->perFrameResources, handle);
    PH_NULL_CHECK(PH_LOG_ERROR, internal);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, !internal->created, PH_ERR_INVALID_STATE);

    internal->data = calloc(PH_MAX_FRAMES_IN_FLIGHT, internal->elemSize);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, internal->data != NULL, PH_ERR_OUT_OF_MEMORY);

    for (int i = 0; i < PH_MAX_FRAMES_IN_FLIGHT; i++)
    {
        void *elem = (uint8_t *)internal->data + i * internal->elemSize;
        PhStatus s = internal->create(hDevice, pCreateParams, (uint32_t)i, elem);
        if (s != PH_SUCCESS)
        {
            for (int j = 0; j < i; j++)
                internal->destroy(hDevice, (uint8_t *)internal->data + j * internal->elemSize);
            free(internal->data);
            internal->data = NULL;
            return s;
        }
    }

    internal->created = true;
    return PH_SUCCESS;
}

PhStatus ph_device_per_frame_destroy(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle)
{
    PhPerFrameResourceInternal *internal = PhPerFrameResourceVec_get(&hDevice->perFrameResources, handle);
    PH_NULL_CHECK(PH_LOG_ERROR, internal);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, internal->created, PH_ERR_INVALID_STATE);

    for (int i = 0; i < PH_MAX_FRAMES_IN_FLIGHT; i++)
        internal->destroy(hDevice, (uint8_t *)internal->data + i * internal->elemSize);

    free(internal->data);
    internal->data    = NULL;
    internal->created = false;
    return PH_SUCCESS;
}

PhStatus ph_device_per_frame_get(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle, void **ppOut)
{
    PH_NULL_CHECK(PH_LOG_ERROR, ppOut);

    PhPerFrameResourceInternal *internal = PhPerFrameResourceVec_get(&hDevice->perFrameResources, handle);
    PH_NULL_CHECK(PH_LOG_ERROR, internal);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, internal->created, PH_ERR_INVALID_STATE);

    uint32_t idx = hDevice->frame % PH_MAX_FRAMES_IN_FLIGHT;
    *ppOut = (uint8_t *)internal->data + idx * internal->elemSize;
    return PH_SUCCESS;
}

PhStatus ph_device_per_frame_get_at(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle, uint32_t index, void **ppOut)
{
    PH_NULL_CHECK(PH_LOG_ERROR, ppOut);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, index < PH_MAX_FRAMES_IN_FLIGHT, PH_ERR_INVALID_ARG);

    PhPerFrameResourceInternal *internal = PhPerFrameResourceVec_get(&hDevice->perFrameResources, handle);
    PH_NULL_CHECK(PH_LOG_ERROR, internal);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, internal->created, PH_ERR_INVALID_STATE);

    *ppOut = (uint8_t *)internal->data + index * internal->elemSize;
    return PH_SUCCESS;
}

PhStatus ph_device_per_frame_unregister(PhDeviceHandle hDevice, PhPerFrameResourceHandle handle)
{
    PhPerFrameResourceInternal *internal = PhPerFrameResourceVec_get(&hDevice->perFrameResources, handle);
    PH_NULL_CHECK(PH_LOG_ERROR, internal);

    if (internal->created)
    {
        for (int i = 0; i < PH_MAX_FRAMES_IN_FLIGHT; i++)
            internal->destroy(hDevice, (uint8_t *)internal->data + i * internal->elemSize);
        free(internal->data);
    }

    memset(internal, 0, sizeof(*internal));
    return PH_SUCCESS;
}