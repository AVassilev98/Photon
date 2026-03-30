#pragma once
#include "cglm/cglm.h"
#include "foundation/span.h"
#include "foundation/vec.h"
#include "photon/photon_device.h"
#include "photon/photon_pipeline.h"
#include "photon/photon_status.h"
#include "assimp/material.h"


typedef struct PhVertex {
    vec3 position;
    vec3 normal;
    vec2 texCoord;
} PhVertex;
FDN_SPAN_DEFINE(PhVertex, PhVertexSpan);
FDN_SPAN_DEFINE(uint32_t, PhIndexSpan);

typedef struct PhSubMesh {
    size_t numIndices;
    uint32_t indicesHandle;
    uint32_t textureHandles[AI_TEXTURE_TYPE_MAX];
} PhSubMesh;

FDN_VEC_DEFINE(PhSubMesh, PhSubMeshVec)
FDN_VEC_DEFINE(PhTexture, MeshTexVec)

typedef struct PhMesh {
    PhVertexSpan vertices;
    PhIndexSpan indices;
    PhBuffer gpuVertexBuffer;
    PhBuffer gpuIndexBuffer;
    PhSubMeshVec subMeshes;
    MeshTexVec textures;
    
} PhMesh;


typedef struct PhRenderObject {
    PhPipeline *pipeline;
    PhMesh       *mesh;
} PhRenderObject;

typedef struct PhRenderObjectCreateInfo {
    PhPipeline *pipeline;
    PhMesh     *mesh;
} PhRenderObjectCreateInfo;
PhStatus ph_render_object_create(PhRenderObjectCreateInfo createInfo, PhRenderObject *out);
PhStatus ph_mesh_create(PhDeviceHandle hDevice, PhVertexSpan vertices, PhIndexSpan indices, PhMesh *out);
PhStatus ph_mesh_create_from_file(PhDeviceHandle hDevice, const char *path, PhMesh *out);