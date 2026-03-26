#pragma once

#include <vulkan/vulkan.h>

typedef enum PhStatus
{
    PH_SUCCESS = 0,

    PH_ERR_UNKNOWN,
    PH_ERR_INVALID_ARG,             /* NULL or out-of-range argument            */
    PH_ERR_OUT_OF_MEMORY,           /* host allocation failed                   */
    PH_ERR_NOT_FOUND,               /* requested object/feature does not exist  */
    PH_ERR_ALREADY_EXISTS,          /* object was already created               */
    PH_ERR_UNSUPPORTED,             /* feature not supported on this platform   */
    PH_ERR_VK,                      /* unclassified VkResult error              */
    PH_ERR_VK_DEVICE_LOST,          /* VK_ERROR_DEVICE_LOST                     */
    PH_ERR_VK_OUT_OF_DEVICE_MEM,    /* VK_ERROR_OUT_OF_DEVICE_MEMORY            */
    PH_ERR_NO_SUITABLE_DEVICE,      /* no GPU meets requirements                */
    PH_ERR_LAYER_NOT_PRESENT,       /* requested validation layer missing       */
    PH_ERR_EXTENSION_NOT_PRESENT,
    PH_ERR_SURFACE_LOST,            /* VK_ERROR_SURFACE_LOST_KHR                */
    PH_ERR_SWAPCHAIN_OUT_OF_DATE,   /* must recreate swapchain                  */
    PH_ERR_SWAPCHAIN_SUBOPTIMAL,    /* presentation succeeded but suboptimal    */
    PH_ERR_BUFFER_TOO_SMALL,
    PH_ERR_IMAGE_FORMAT_UNSUPPORTED,
    PH_ERR_MAPPING_FAILED,          /* vkMapMemory failed                       */
    PH_ERR_SHADER_COMPILE,          /* SPIR-V invalid or compilation failed     */
    PH_ERR_PIPELINE_COMPILE,        /* pipeline creation failed                 */
    PH_ERR_TIMEOUT,                 /* fence/semaphore wait timed out           */
    PH_ERR_SYNC_OBJECT_INVALID,
    PH_ERR_INVALID_STATE,

} PhStatus;

/* Convert a VkResult to the closest PhStatus. */
PhStatus    ph_status_from_vk(VkResult vk);

/* Human-readable string for a PhStatus code. Never returns NULL. */
const char *ph_status_str(PhStatus s);
