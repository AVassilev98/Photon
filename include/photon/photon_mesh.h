#pragma once
#include "cglm/cglm.h"
#include "photon/photon_device.h"
#include "photon/photon_pipeline.h"
#include "photon/photon_status.h"

typedef struct PhVertex {
    vec2 position;
    vec3 color;
} PhVertex;

typedef struct PhMesh {
    PhVertex *vertices;
    PhBuffer gpuVertexBuffer;
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
PhStatus ph_mesh_create(PhDeviceHandle hDevice, PhVertex *vertices, uint32_t count, PhMesh *out);
PhStatus ph_mesh_create_from_file(PhDeviceHandle hDevice, const char *path, PhMesh *out);