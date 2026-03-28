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

PhStatus ph_mesh_create(PhDeviceHandle hDevice, PhVertexSpan vertices, PhIndexSpan indices, PhMesh *out)
{
    PhBuffer vertexBuffer;
    PhBuffer indexBuffer;

    PH_CHECK(PH_LOG_ERROR, 
        ph_device_buffer_create(hDevice, PH_QUEUE_TYPE_GRAPHICS_BIT | PH_QUEUE_TYPE_TRANSFER_BIT, vertices.len * sizeof(PhVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &vertexBuffer, 0));
    PH_CHECK(PH_LOG_ERROR, 
        ph_device_buffer_upload(hDevice, vertices.ptr, vertices.len * sizeof(PhVertex), vertexBuffer));

    PH_CHECK(PH_LOG_ERROR, 
        ph_device_buffer_create(hDevice, PH_QUEUE_TYPE_GRAPHICS_BIT | PH_QUEUE_TYPE_TRANSFER_BIT, indices.len * sizeof(uint16_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &indexBuffer, 0));
    PH_CHECK(PH_LOG_ERROR, 
        ph_device_buffer_upload(hDevice, indices.ptr, indices.len * sizeof(uint16_t), indexBuffer));


    *out = (PhMesh) {
        .vertices = vertices,
        .gpuVertexBuffer = vertexBuffer,
        .indices = indices,
        .gpuIndexBuffer = indexBuffer,
    };

    return PH_SUCCESS;
}

PhStatus ph_mesh_create_from_file(PhDeviceHandle hDevice, const char *path, PhMesh *out)
{
    // TODO: Fill in this function

    return PH_SUCCESS;
}