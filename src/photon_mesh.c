#include "photon/photon_mesh.h"
#include "photon/photon_device.h"
#include "photon/photon_error.h"
#include "photon/photon_status.h"

PhStatus ph_render_object_create(PhRenderObjectCreateInfo createInfo, PhRenderObject *out)
{
    *out = (PhRenderObject){
        .mesh = createInfo.mesh,
        .pipeline = createInfo.pipeline
    };

    return PH_SUCCESS;
}

PhStatus ph_mesh_create(PhDeviceHandle hDevice, PhVertex *vertices, uint32_t count, PhMesh *out)
{
    PhBuffer buffer;
    PH_CHECK(PH_LOG_ERROR, 
        ph_device_buffer_create(hDevice, PH_QUEUE_TYPE_GRAPHICS_BIT | PH_QUEUE_TYPE_TRANSFER_BIT, count * sizeof(PhVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &buffer, 0));
    PH_CHECK(PH_LOG_ERROR, 
        ph_device_buffer_upload(hDevice, vertices, count * sizeof(PhVertex), buffer));

    *out = (PhMesh) {
        .vertices = vertices,
        .gpuVertexBuffer = buffer
    };

    return PH_SUCCESS;
}

PhStatus ph_mesh_create_from_file(PhDeviceHandle hDevice, const char *path, PhMesh *out)
{
    // TODO: Fill in this function

    return PH_SUCCESS;
}