#include "photon/photon_mesh.h"
#include "assimp/material.h"
#include "assimp/types.h"
#include "foundation/threadpool.h"
#include "foundation/vec.h"
#include "photon/photon_device.h"
#include "photon/photon_error.h"
#include "photon/photon_log.h"
#include "photon/photon_status.h"
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_SHARING_MODE_EXCLUSIVE, &vertexBuffer, 0));
    PH_CHECK(PH_LOG_ERROR,
        ph_device_buffer_upload(hDevice, vertices.ptr, vertices.len * sizeof(PhVertex), vertexBuffer));

    PH_CHECK(PH_LOG_ERROR,
        ph_device_buffer_create(hDevice, PH_QUEUE_TYPE_GRAPHICS_BIT | PH_QUEUE_TYPE_TRANSFER_BIT, indices.len * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_SHARING_MODE_EXCLUSIVE, &indexBuffer, 0));
    PH_CHECK(PH_LOG_ERROR,
        ph_device_buffer_upload(hDevice, indices.ptr, indices.len * sizeof(uint32_t), indexBuffer));


    out->vertices        = vertices;
    out->gpuVertexBuffer = vertexBuffer;
    out->indices         = indices;
    out->gpuIndexBuffer  = indexBuffer;

    return PH_SUCCESS;
}

typedef struct MmapImage
{
    void   *data;
    size_t  size;
} MmapImage;

static MmapImage mmapPngImage(const char *path)
{
    MmapImage result = { .data = NULL, .size = 0 };

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        PH_LOG_WARN("Failed to open texture file: %s", path);
        return result;
    }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        PH_LOG_WARN("Failed to stat texture file: %s", path);
        close(fd);
        return result;
    }

    void *mapped = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapped == MAP_FAILED)
    {
        PH_LOG_WARN("Failed to mmap texture file: %s", path);
        return result;
    }

    madvise(mapped, (size_t)st.st_size, MADV_WILLNEED);

    result.data = mapped;
    result.size = (size_t)st.st_size;
    return result;
}

typedef struct TextureLoadParams
{
    PhMaterial      *material;
    PhDeviceHandle  hDevice;
    char            path[512];
    unsigned        textureType;
    pthread_mutex_t *texVecMutex;
} TextureLoadParams;
FDN_VEC_DEFINE(TextureLoadParams, TextureLoadParamVec);

static void _texture_data_load(void *data)
{
    TextureLoadParams *params = (TextureLoadParams *)data;
    int w, h, channels;

    MmapImage img = mmapPngImage(params->path);
    if (!img.data)
    {
        PH_LOG_WARN("Failed to load texture: %s", params->path);
        return;
    }

    stbi_uc *pixels = stbi_load_from_memory(img.data, (int)img.size, &w, &h, &channels, 4);
    munmap(img.data, img.size);

    // stbi_uc *pixels = stbi_load(params->path, &w, &h, &channels, 4);

    if (pixels)
    {
        PhTextureCreateInfo texInfo = {
            .format   = VK_FORMAT_R8G8B8A8_SRGB,
            .data     = pixels,
            .width    = (uint32_t)w,
            .height   = (uint32_t)h,
            .elemSize = 4,
        };
        PhTexture texture = { 0 };
        pthread_mutex_lock(params->texVecMutex);
        PhStatus texStatus = ph_device_texture_create(params->hDevice, &texInfo, &texture);
        stbi_image_free(pixels);

        if (texStatus == PH_SUCCESS)
        {
            params->material->textureHandles[params->textureType] = params->material->textures.len;
            MatTexVec_push(&params->material->textures, texture);
        }
        pthread_mutex_unlock(params->texVecMutex);
    }
    else
    {
        PH_LOG_WARN("Failed to decode texture: %s", params->path);
    }
}

PhStatus ph_mesh_create_from_file(PhDeviceHandle hDevice, const char *path, PhMesh *out)
{
    PH_NULL_CHECK(PH_LOG_ERROR, path);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    FdnThreadPool threadPool;
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    fdn_threadpool_init(&threadPool, nprocs);
    TextureLoadParamVec paramVec;
    TextureLoadParamVec_init(&paramVec);

    pthread_mutex_t texVecMutex = PTHREAD_MUTEX_INITIALIZER;

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

    MaterialVec_init(&out->materials);
    PhSubMeshVec_init(&out->subMeshes);
    MaterialVec_reserve(&out->materials, scene->mNumMaterials);
    PhSubMeshVec_reserve(&out->subMeshes, scene->mNumMeshes);

    PhMaterial *materials = calloc(scene->mNumMaterials, sizeof(PhMaterial));
    if (!materials)
    {
        free(vertices);
        free(indices);
        aiReleaseImport(scene);
        return PH_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        MatTexVec_init(&materials[i].textures);
    }

    const char *lastSlash = strrchr(path, '/');
    size_t dirLen = lastSlash ? (size_t)(lastSlash - path + 1) : 0;

    uint32_t maxJobs = scene->mNumMaterials * AI_TEXTURE_TYPE_MAX;
    TextureLoadParamVec_reserve(&paramVec, maxJobs);

    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        const struct aiMaterial *mat = scene->mMaterials[i];
        struct aiString matName;
        aiGetMaterialString(mat, AI_MATKEY_NAME, &matName);
        PH_LOG_INFO("Loading textures for material: %s", matName.data);
        for (uint32_t j = 0; j < AI_TEXTURE_TYPE_MAX; j++)
        {
            struct aiString texPath;
            if (aiGetMaterialTexture(mat, j, 0, &texPath,
                        NULL, NULL, NULL, NULL, NULL, NULL) != aiReturn_SUCCESS)
            {
                continue;
            }

            PH_LOG_INFO("\t Found %s texture for material: %s", aiTextureTypeToString(j), matName.data);

            TextureLoadParams loadParams = {
                .textureType = j,
                .hDevice = hDevice,
                .material = &materials[i],
                .texVecMutex = &texVecMutex,
            };
            snprintf(loadParams.path, sizeof(loadParams.path), "%.*s%s", (int)dirLen, path, texPath.data);

            TextureLoadParamVec_push(&paramVec, loadParams);
            fdn_threadpool_submit(&threadPool, _texture_data_load, &paramVec.data[paramVec.len - 1]);
        }
    }

    fdn_threadpool_wait(&threadPool);

    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
        MaterialVec_push(&out->materials, materials[i]);
    {
        free(materials);
    }

    uint32_t vertOff = 0;
    uint32_t idxOff  = 0;
    for (unsigned m = 0; m < scene->mNumMeshes; m++)
    {
        const struct aiMesh *aimesh = scene->mMeshes[m];
        PH_LOG_INFO("Loading assimp submesh %d", m);
        PhSubMesh submesh = {
            .indicesHandle  = idxOff,
            .materialHandle = aimesh->mMaterialIndex,
        };

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
        submesh.numIndices = idxOff - submesh.indicesHandle;

        PhSubMeshVec_push(&out->subMeshes, submesh);
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