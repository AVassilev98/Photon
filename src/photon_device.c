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
            graphicsFamily = i;
        if (computeFamily == UINT32_MAX && (f & VK_QUEUE_COMPUTE_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT))
            computeFamily = i;
        if (transferFamily == UINT32_MAX && (f & VK_QUEUE_TRANSFER_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT) && !(f & VK_QUEUE_COMPUTE_BIT))
            transferFamily = i;
    }

    float                  queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfos[3];
    uint32_t                queueInfoCount = 0;

    if (graphicsFamily != UINT32_MAX)
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = graphicsFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
    if (computeFamily != UINT32_MAX)
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = computeFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
    if (transferFamily != UINT32_MAX)
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo){
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = transferFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };

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
    }

cleanup:
    PH_FREE_IF_SET(pSurfaceFormats);
    PH_FREE_IF_SET(pPresentModes);
    return status;
}