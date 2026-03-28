#include "photon/photon_ubo.h"
#include "photon/photon_error.h"
#include "photon/photon_log.h"
#include <vulkan/vulkan_core.h>

static PhStatus _ubo_create_callback(PhDeviceHandle hDevice, void *userdata, uint32_t frameIndex, void *out)
{
    (void)frameIndex;
    PhUBOCreateInfo *params = (PhUBOCreateInfo *)userdata;
    PhUBO *ubo = (PhUBO *)out;

    return ph_ubo_create(hDevice, params, ubo);
}

static void _ubo_destroy_callback(PhDeviceHandle hDevice, void *resource)
{
    PhUBO *ubo = (PhUBO *)resource;
    ph_ubo_destroy(hDevice, ubo);
}

PhStatus ph_ubo_create(PhDeviceHandle hDevice, PhUBOCreateInfo *pParams, PhUBO *pOut)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pParams);
    PH_NULL_CHECK(PH_LOG_ERROR, pOut);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, pParams->size > 0, PH_ERR_INVALID_ARG);

    PH_CHECK(PH_LOG_ERROR,
        ph_device_buffer_create(hDevice,
            PH_QUEUE_TYPE_GRAPHICS_BIT,
            pParams->size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            &pOut->ubo,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT));

    PH_CHECK(PH_LOG_ERROR, ph_device_buffer_map(hDevice, &pOut->ubo));

    return PH_SUCCESS;
}

PhStatus ph_ubo_destroy(PhDeviceHandle hDevice, PhUBO *pUbo)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pUbo);

    return ph_device_buffer_destroy(hDevice, &pUbo->ubo);
}

PhStatus ph_ubo_per_frame_create(PhDeviceHandle hDevice, PhUBOCreateInfo *pParams, PhUBOPerFrame *pOut)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pParams);
    PH_NULL_CHECK(PH_LOG_ERROR, pOut);

    PH_CHECK(PH_LOG_ERROR,
        ph_device_per_frame_register(hDevice,
            sizeof(PhUBO),
            _ubo_create_callback,
            _ubo_destroy_callback,
            NULL,
            &pOut->uboDataHandle));

    PH_CHECK(PH_LOG_ERROR,
        ph_device_per_frame_create(hDevice, pOut->uboDataHandle, pParams));

    return PH_SUCCESS;
}

PhStatus ph_ubo_per_frame_get(PhDeviceHandle hDevice, PhUBOPerFrame *pUboPerFrame, PhUBO **ppOut)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pUboPerFrame);
    PH_NULL_CHECK(PH_LOG_ERROR, ppOut);

    return ph_device_per_frame_get(hDevice, pUboPerFrame->uboDataHandle, (void **)ppOut);
}

PhStatus ph_ubo_per_frame_destroy(PhDeviceHandle hDevice, PhUBOPerFrame *pUboPerFrame)
{
    PH_NULL_CHECK(PH_LOG_ERROR, pUboPerFrame);

    PH_CHECK(PH_LOG_ERROR,
        ph_device_per_frame_destroy(hDevice, pUboPerFrame->uboDataHandle));
    PH_CHECK(PH_LOG_ERROR,
        ph_device_per_frame_unregister(hDevice, pUboPerFrame->uboDataHandle));

    return PH_SUCCESS;
}
