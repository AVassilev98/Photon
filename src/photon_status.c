#include <photon/photon_status.h>
#include <vulkan/vulkan.h>

PhStatus ph_status_from_vk(VkResult vk)
{
    switch (vk)
    {
        case VK_SUCCESS:                        return PH_SUCCESS;
        case VK_TIMEOUT:                        return PH_ERR_TIMEOUT;
        case VK_SUBOPTIMAL_KHR:                 return PH_ERR_SWAPCHAIN_SUBOPTIMAL;
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return PH_ERR_OUT_OF_MEMORY;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return PH_ERR_VK_OUT_OF_DEVICE_MEM;
        case VK_ERROR_DEVICE_LOST:              return PH_ERR_VK_DEVICE_LOST;
        case VK_ERROR_SURFACE_LOST_KHR:         return PH_ERR_SURFACE_LOST;
        case VK_ERROR_OUT_OF_DATE_KHR:          return PH_ERR_SWAPCHAIN_OUT_OF_DATE;
        case VK_ERROR_LAYER_NOT_PRESENT:        return PH_ERR_LAYER_NOT_PRESENT;
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return PH_ERR_EXTENSION_NOT_PRESENT;
        case VK_ERROR_FEATURE_NOT_PRESENT:      return PH_ERR_UNSUPPORTED;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return PH_ERR_IMAGE_FORMAT_UNSUPPORTED;
        case VK_ERROR_MEMORY_MAP_FAILED:        return PH_ERR_MAPPING_FAILED;
        case VK_ERROR_TOO_MANY_OBJECTS:
        case VK_ERROR_INCOMPATIBLE_DRIVER:
        case VK_ERROR_INITIALIZATION_FAILED:
        default:                                return PH_ERR_VK;
    }
}

const char *ph_status_str(PhStatus s)
{
    switch (s)
    {
        case PH_SUCCESS:                        return "PH_SUCCESS";
        case PH_ERR_UNKNOWN:                    return "PH_ERR_UNKNOWN";
        case PH_ERR_INVALID_ARG:                return "PH_ERR_INVALID_ARG";
        case PH_ERR_OUT_OF_MEMORY:              return "PH_ERR_OUT_OF_MEMORY";
        case PH_ERR_NOT_FOUND:                  return "PH_ERR_NOT_FOUND";
        case PH_ERR_ALREADY_EXISTS:             return "PH_ERR_ALREADY_EXISTS";
        case PH_ERR_UNSUPPORTED:                return "PH_ERR_UNSUPPORTED";
        case PH_ERR_VK:                         return "PH_ERR_VK";
        case PH_ERR_VK_DEVICE_LOST:             return "PH_ERR_VK_DEVICE_LOST";
        case PH_ERR_VK_OUT_OF_DEVICE_MEM:       return "PH_ERR_VK_OUT_OF_DEVICE_MEM";
        case PH_ERR_NO_SUITABLE_DEVICE:         return "PH_ERR_NO_SUITABLE_DEVICE";
        case PH_ERR_LAYER_NOT_PRESENT:          return "PH_ERR_LAYER_NOT_PRESENT";
        case PH_ERR_EXTENSION_NOT_PRESENT:      return "PH_ERR_EXTENSION_NOT_PRESENT";
        case PH_ERR_SURFACE_LOST:               return "PH_ERR_SURFACE_LOST";
        case PH_ERR_SWAPCHAIN_OUT_OF_DATE:      return "PH_ERR_SWAPCHAIN_OUT_OF_DATE";
        case PH_ERR_SWAPCHAIN_SUBOPTIMAL:       return "PH_ERR_SWAPCHAIN_SUBOPTIMAL";
        case PH_ERR_BUFFER_TOO_SMALL:           return "PH_ERR_BUFFER_TOO_SMALL";
        case PH_ERR_IMAGE_FORMAT_UNSUPPORTED:   return "PH_ERR_IMAGE_FORMAT_UNSUPPORTED";
        case PH_ERR_MAPPING_FAILED:             return "PH_ERR_MAPPING_FAILED";
        case PH_ERR_SHADER_COMPILE:             return "PH_ERR_SHADER_COMPILE";
        case PH_ERR_PIPELINE_COMPILE:           return "PH_ERR_PIPELINE_COMPILE";
        case PH_ERR_TIMEOUT:                    return "PH_ERR_TIMEOUT";
        case PH_ERR_SYNC_OBJECT_INVALID:        return "PH_ERR_SYNC_OBJECT_INVALID";
        default:                                return "<invalid PhStatus>";
    }
}
