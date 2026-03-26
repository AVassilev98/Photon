#include "photon/photon_device.h"
#include "photon/photon_pipeline.h"
#include "photon_device_internal.h"
#include "photon_instance_internal.h"
#include "photon/photon_error.h"
#include "photon/photon_log.h"
#include "photon/photon_status.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>
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
    if (computeFamily != UINT32_MAX)
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
    if (transferFamily != UINT32_MAX)
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
    
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateCommandPool(pDeviceInfo->handle->device, &commandPoolInfos[0], NULL, &pDeviceInfo->handle->graphicsPool), status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateCommandPool(pDeviceInfo->handle->device, &commandPoolInfos[1], NULL, &pDeviceInfo->handle->computePool), status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateCommandPool(pDeviceInfo->handle->device, &commandPoolInfos[2], NULL, &pDeviceInfo->handle->transferPool), status, exit);

    vkGetDeviceQueue(pDeviceInfo->handle->device, graphicsFamily, 0, &pDeviceInfo->handle->graphicsQueue);
    if (computeFamily  != UINT32_MAX)
        vkGetDeviceQueue(pDeviceInfo->handle->device, computeFamily,  0, &pDeviceInfo->handle->computeQueue);
    if (transferFamily != UINT32_MAX)
        vkGetDeviceQueue(pDeviceInfo->handle->device, transferFamily, 0, &pDeviceInfo->handle->transferQueue);
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

PhStatus ph_enumerate_devices(PhInstanceHandle hInstance, PhCapability caps, PhDeviceInfoSpan *pDeviceInfoSpan)
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

    for (size_t i = 0; i < physDeviceCount; i++)
    {
        bool deviceMeetsRequirements = false;
        PH_PROPAGATE_GOTO(PH_LOG_ERROR, _phys_device_meets_requirements(pPhysicalDevices[i], caps, &deviceMeetsRequirements), status, exit);
        if (deviceMeetsRequirements)
        {
            PH_PROPAGATE_GOTO(PH_LOG_ERROR, _initialize_ph_device_info(pPhysicalDevices[i], caps, &pDeviceInfos[deviceInfoCount]), status, exit);
            PH_LOG_INFO("Initialized device: %s", pDeviceInfos[deviceInfoCount].pName);
            
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


PhStatus ph_configure_device_for_present(PhDeviceHandle hDevice, PhSurfaceHandle hSurface, PhPresentOptions opts)
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

    PH_VK_CHECK_GOTO(PH_LOG_ERROR,
        vkGetSwapchainImagesKHR(hDevice->device, hDevice->swapchain, &hDevice->swapchainImageCount, NULL),
        status, cleanup);
    PH_MALLOC_GOTO(PH_LOG_ERROR, hDevice->pSwapchainImages, sizeof(VkImage) * hDevice->swapchainImageCount, status, cleanup);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR,
        vkGetSwapchainImagesKHR(hDevice->device, hDevice->swapchain, &hDevice->swapchainImageCount, hDevice->pSwapchainImages),
        status, cleanup);

    PH_MALLOC_GOTO(PH_LOG_ERROR, hDevice->pSwapchainImageViews, sizeof(VkImageView) * hDevice->swapchainImageCount, status, cleanup);
    PH_MALLOC_GOTO(PH_LOG_ERROR, hDevice->pPresentSemaphores, sizeof(VkSemaphore) * hDevice->swapchainImageCount, status, cleanup);
    PH_MALLOC_GOTO(PH_LOG_ERROR, hDevice->pRenderSemaphores,  sizeof(VkSemaphore) * hDevice->swapchainImageCount, status, cleanup);
    PH_MALLOC_GOTO(PH_LOG_ERROR, hDevice->pPresentFences, sizeof(VkFence) * hDevice->swapchainImageCount, status, cleanup);

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
        PH_VK_CHECK_GOTO(PH_LOG_ERROR,
            vkCreateImageView(hDevice->device, &viewInfo, NULL, &hDevice->pSwapchainImageViews[i]),
            status, cleanup);

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .flags = 0,
            .pNext = NULL,
        };
        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            .pNext = NULL,
        };
        PH_VK_CHECK_GOTO(PH_LOG_ERROR,
            vkCreateSemaphore(hDevice->device, &semaphoreCreateInfo, NULL, &hDevice->pPresentSemaphores[i]), status, cleanup);
        PH_VK_CHECK_GOTO(PH_LOG_ERROR,
            vkCreateSemaphore(hDevice->device, &semaphoreCreateInfo, NULL, &hDevice->pRenderSemaphores[i]), status, cleanup);
        PH_VK_CHECK_GOTO(PH_LOG_ERROR,
            vkCreateFence(hDevice->device, &fenceCreateInfo, NULL, &hDevice->pPresentFences[i]), status, cleanup);
    }

cleanup:
    PH_FREE_IF_SET(pSurfaceFormats);
    PH_FREE_IF_SET(pPresentModes);
    return status;
}

PhStatus ph_device_command_buffer_create(PhDeviceHandle hDevice, PhCommandBufferType type, size_t count, PhCommandBuffer *pBuffers)
{
    VkCommandPool commandPool = { 0 };
    VkCommandBufferAllocateInfo bufferAllocateInfo = { 0 };

    PH_NULL_CHECK(PH_LOG_ERROR, pBuffers);
    
    switch (type)
    {
        case PH_COMMAND_BUFFER_TYPE_GRAPHICS:
            commandPool = hDevice->graphicsPool;
            break;
        case PH_COMMAND_BUFFER_TYPE_COMPUTE:
            commandPool = hDevice->computePool;
            break;
        case PH_COMMAND_BUFFER_TYPE_TRANSFER:
            commandPool = hDevice->transferPool;
            break;
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

PhStatus ph_device_command_buffer_destroy(PhDeviceHandle hDevice, PhCommandBufferType type, size_t count, PhCommandBuffer *pBuffers)
{
    VkCommandPool commandPool = { 0 };
    VkCommandBufferAllocateInfo bufferAllocateInfo = { 0 };
    
    switch (type)
    {
        case PH_COMMAND_BUFFER_TYPE_GRAPHICS:
            commandPool = hDevice->graphicsPool;
            break;
        case PH_COMMAND_BUFFER_TYPE_COMPUTE:
            commandPool = hDevice->computePool;
            break;
        case PH_COMMAND_BUFFER_TYPE_TRANSFER:
            commandPool = hDevice->transferPool;
            break;
    };

    vkFreeCommandBuffers(hDevice->device, commandPool, count, pBuffers);
    return PH_SUCCESS;
}

PhStatus ph_device_present(PhDeviceHandle hDevice, struct PhPipeline *pPipeline)
{
    static size_t frame = 0;
    PhCommandBuffer buffer = { 0 };
    size_t currentImageIndex = frame % hDevice->swapchainImageCount;
    uint32_t imageIndex = 0;

    PH_VK_CHECK(PH_LOG_ERROR,
        vkWaitForFences(hDevice->device, 1, &hDevice->pPresentFences[currentImageIndex], VK_TRUE, UINT64_MAX));
    PH_VK_CHECK(PH_LOG_ERROR,
        vkResetFences(hDevice->device, 1, &hDevice->pPresentFences[currentImageIndex]));

    PH_VK_CHECK(PH_LOG_ERROR,
        vkAcquireNextImageKHR(hDevice->device, hDevice->swapchain, UINT64_MAX, hDevice->pPresentSemaphores[currentImageIndex], VK_NULL_HANDLE, &imageIndex));

    PH_CHECK(PH_LOG_ERROR, ph_device_command_buffer_create(hDevice, PH_COMMAND_BUFFER_TYPE_GRAPHICS, 1, &buffer));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    PH_VK_CHECK(PH_LOG_ERROR, vkBeginCommandBuffer(buffer, &beginInfo));

    VkImageMemoryBarrier toRender = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = hDevice->pSwapchainImages[imageIndex],
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    vkCmdPipelineBarrier(buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, NULL, 0, NULL, 1, &toRender);

    VkRenderingAttachmentInfo colorAttachment = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = hDevice->pSwapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } },
    };
    VkRenderingInfo renderingInfo = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = { .offset = { 0, 0 }, .extent = hDevice->swapchainExtent },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
    };

    vkCmdBeginRendering(buffer, &renderingInfo);

    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline->pipeline);

    VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)hDevice->swapchainExtent.width,
        .height   = (float)hDevice->swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor = { .offset = { 0, 0 }, .extent = hDevice->swapchainExtent };
    vkCmdSetScissor(buffer, 0, 1, &scissor);

    vkCmdDraw(buffer, 3, 1, 0, 0);

    vkCmdEndRendering(buffer);

    VkImageMemoryBarrier toPresent = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask       = 0,
        .oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = hDevice->pSwapchainImages[imageIndex],
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
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &hDevice->pPresentSemaphores[currentImageIndex],
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
        .pImageIndices      = &imageIndex,
    };
    PH_VK_CHECK(PH_LOG_ERROR,
        vkQueuePresentKHR(hDevice->graphicsQueue, &presentInfo));

    frame++;
    return PH_SUCCESS;
}