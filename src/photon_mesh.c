#include "photon/photon_mesh.h"
#include "photon/photon_device.h"
#include "photon/photon_error.h"
#include "photon/photon_log.h"
#include "photon/photon_status.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdlib.h>
#include <string.h>

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
        ph_device_buffer_create(hDevice, PH_QUEUE_TYPE_GRAPHICS_BIT | PH_QUEUE_TYPE_TRANSFER_BIT, indices.len * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, &indexBuffer, 0));
    PH_CHECK(PH_LOG_ERROR,
        ph_device_buffer_upload(hDevice, indices.ptr, indices.len * sizeof(uint32_t), indexBuffer));


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
    PH_NULL_CHECK(PH_LOG_ERROR, path);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    const struct aiScene *scene = aiImportFile(path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices);

    PH_CHECK_OR_RETURN(PH_LOG_ERROR, scene != NULL, PH_ERR_INVALID_ARG);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, scene->mNumMeshes > 0, PH_ERR_INVALID_ARG);

    /* Count total vertices and indices across all meshes */
    uint32_t totalVertices = 0;
    uint32_t totalIndices  = 0;
    for (unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        const struct aiMesh *aimesh = scene->mMeshes[m];
        totalVertices += aimesh->mNumVertices;
        for (unsigned f = 0; f < aimesh->mNumFaces; f++)
            totalIndices += aimesh->mFaces[f].mNumIndices;
    }

    PH_LOG_INFO("Loading %s: %u meshes, %u vertices, %u indices",
                path, scene->mNumMeshes, totalVertices, totalIndices);

    PhVertex *vertices = malloc(totalVertices * sizeof(PhVertex));
    uint32_t *indices  = malloc(totalIndices  * sizeof(uint32_t));
    if (!vertices || !indices)
    {
        free(vertices);
        free(indices);
        aiReleaseImport(scene);
        return PH_ERR_OUT_OF_MEMORY;
    }

    uint32_t vertOff = 0;
    uint32_t idxOff  = 0;
    for (unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        const struct aiMesh *aimesh = scene->mMeshes[m];

        for (unsigned v = 0; v < aimesh->mNumVertices; v++)
        {
            PhVertex *dst = &vertices[vertOff + v];
            dst->position[0] = aimesh->mVertices[v].x;
            dst->position[1] = aimesh->mVertices[v].y;
            dst->position[2] = aimesh->mVertices[v].z;

            if (aimesh->mNormals)
            {
                dst->normal[0] = aimesh->mNormals[v].x;
                dst->normal[1] = aimesh->mNormals[v].y;
                dst->normal[2] = aimesh->mNormals[v].z;
            }
            else
            {
                memset(dst->normal, 0, sizeof(dst->normal));
            }

            if (aimesh->mTextureCoords[0])
            {
                dst->texCoord[0] = aimesh->mTextureCoords[0][v].x;
                dst->texCoord[1] = aimesh->mTextureCoords[0][v].y;
            }
            else
            {
                memset(dst->texCoord, 0, sizeof(dst->texCoord));
            }
        }

        for (unsigned f = 0; f < aimesh->mNumFaces; f++)
        {
            const struct aiFace *face = &aimesh->mFaces[f];
            for (unsigned i = 0; i < face->mNumIndices; i++)
                indices[idxOff++] = vertOff + face->mIndices[i];
        }

        vertOff += aimesh->mNumVertices;
    }

    aiReleaseImport(scene);

    PhVertexSpan vertSpan = PhVertexSpan_from(vertices, totalVertices);
    PhIndexSpan  idxSpan  = PhIndexSpan_from(indices, totalIndices);

    PhStatus status = ph_mesh_create(hDevice, vertSpan, idxSpan, out);

    free(vertices);
    free(indices);

    return status;
}