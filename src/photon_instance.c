#include "photon_instance_internal.h"
#include "photon_window_internal.h"
#include "photon/photon_error.h"

#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

static const char *s_validationLayers[] = {
    "VK_LAYER_KHRONOS_validation",
};

static bool isValidationLayer(const char *name)
{
    for (size_t i = 0; i < PH_NUM_ELEMS(s_validationLayers); i++)
        if (!strcmp(name, s_validationLayers[i]))
            return true;
    return false;
}


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             type,
    const VkDebugUtilsMessengerCallbackDataEXT *data,
    void                                       *userData)
{
    (void)type;
    (void)userData;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        PH_LOG_ERROR("[Vulkan] %s", data->pMessage);
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        PH_LOG_WARN("[Vulkan] %s", data->pMessage);
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        PH_LOG_VERBOSE("[Vulkan] %s", data->pMessage);
    else
        PH_LOG_VERBOSE("[Vulkan] %s", data->pMessage);

    return VK_FALSE;
}

static VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerInfo(void)
{
    return (VkDebugUtilsMessengerCreateInfoEXT){
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
    };
}


PhStatus ph_create_instance(PhInstanceSettings *settings, PhInstanceHandle *out)
{
    PhStatus            status                   = PH_SUCCESS;
    PhInstance         *pInstance                = NULL;
    const char        **ppLayerNames             = NULL;
    VkLayerProperties  *pLayerProperties         = NULL;
    const char        **ppExtensionNames         = NULL;
    uint32_t            enabledLayerCount        = 0;
    uint32_t            enabledExtensionCount    = 0;
    uint32_t            vkLayerCount             = 0;
    uint32_t            windowExtensionCount     = 0;

    PH_NULL_CHECK(PH_LOG_ERROR, settings);
    PH_NULL_CHECK(PH_LOG_ERROR, settings->appName);

    if (settings->enableDebug)
    {
        PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateInstanceLayerProperties(&vkLayerCount, NULL), status, exit);
        pLayerProperties = malloc(sizeof(VkLayerProperties) * vkLayerCount);
        PH_CHECK_GOTO(PH_LOG_ERROR, pLayerProperties, PH_ERR_OUT_OF_MEMORY, status, exit);
        PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkEnumerateInstanceLayerProperties(&vkLayerCount, pLayerProperties), status, exit);

        ppLayerNames = malloc(sizeof(char *) * PH_NUM_ELEMS(s_validationLayers));
        PH_CHECK_GOTO(PH_LOG_ERROR, ppLayerNames, PH_ERR_OUT_OF_MEMORY, status, exit);

        PH_LOG_INFO("Validation layers:");
        for (uint32_t i = 0; i < vkLayerCount; i++)
        {
            PH_LOG_VERBOSE("  found: %s", pLayerProperties[i].layerName);
            if (isValidationLayer(pLayerProperties[i].layerName))
            {
                PH_LOG_INFO("  enabled: %s", pLayerProperties[i].layerName);
                ppLayerNames[enabledLayerCount++] = pLayerProperties[i].layerName;
            }
        }
    }
    glfwInit();

    PH_PROPAGATE_GOTO(PH_LOG_ERROR, ph_window_get_required_extensions(NULL, &windowExtensionCount), status, exit);

    uint32_t extraExtensions = settings->enableDebug ? 1 : 0; /* debug utils */
    ppExtensionNames = malloc(sizeof(char *) * (windowExtensionCount + extraExtensions));
    PH_CHECK_GOTO(PH_LOG_ERROR, ppExtensionNames, PH_ERR_OUT_OF_MEMORY, status, exit);

    PH_PROPAGATE_GOTO(PH_LOG_ERROR, ph_window_get_required_extensions(ppExtensionNames, &windowExtensionCount), status, exit);
    enabledExtensionCount = windowExtensionCount;

    if (settings->enableDebug)
        ppExtensionNames[enabledExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    VkApplicationInfo appInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = settings->appName,
        .applicationVersion = settings->appVersion,
        .pEngineName        = "Photon",
        .engineVersion      = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion         = VK_API_VERSION_1_4,
    };

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = makeDebugMessengerInfo();

    VkInstanceCreateInfo instanceInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = settings->enableDebug ? &messengerInfo : NULL,
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = enabledLayerCount,
        .ppEnabledLayerNames     = ppLayerNames,
        .enabledExtensionCount   = enabledExtensionCount,
        .ppEnabledExtensionNames = ppExtensionNames,
    };

    pInstance = calloc(1, sizeof(PhInstance));
    PH_CHECK_GOTO(PH_LOG_ERROR, pInstance, PH_ERR_OUT_OF_MEMORY, status, exit);
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateInstance(&instanceInfo, NULL, &pInstance->instance), status, exit);

    if (settings->enableDebug)
    {
        PFN_vkCreateDebugUtilsMessengerEXT fn =
            (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(pInstance->instance, "vkCreateDebugUtilsMessengerEXT");

        PH_CHECK_GOTO(PH_LOG_ERROR, fn, PH_ERR_EXTENSION_NOT_PRESENT, status, exit);
        PH_VK_CHECK_GOTO(PH_LOG_ERROR, fn(pInstance->instance, &messengerInfo, NULL, &pInstance->debugMessenger), status, exit);
    }

exit:
    PH_FREE_IF_SET(pLayerProperties);
    PH_FREE_IF_SET(ppLayerNames);
    PH_FREE_IF_SET(ppExtensionNames);

    if (status != PH_SUCCESS)
    {
        if (pInstance && pInstance->instance)
            vkDestroyInstance(pInstance->instance, NULL);
        PH_FREE_IF_SET(pInstance);
        return status;
    }

    *out = pInstance;
    return PH_SUCCESS;
}

PhStatus ph_destroy_instance(PhInstanceHandle handle)
{
    PH_NULL_CHECK(PH_LOG_ERROR, handle);

    if (handle->debugMessenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT fn =
            (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(handle->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn)
            fn(handle->instance, handle->debugMessenger, NULL);
    }

    vkDestroyInstance(handle->instance, NULL);
    free(handle);
    return PH_SUCCESS;
}
