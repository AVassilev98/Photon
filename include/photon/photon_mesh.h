#pragma once
#include "cglm/cglm.h"
#include "foundation/span.h"
#include "photon/photon_device.h"
#include "photon/photon_pipeline.h"
#include "photon/photon_status.h"


typedef struct PhVertex {
    vec2 position;
    vec3 color;
} PhVertex;
FDN_SPAN_DEFINE(PhVertex, PhVertexSpan);
FDN_SPAN_DEFINE(uint16_t, PhIndexSpan);

typedef struct PhMesh {
    PhVertexSpan vertices;
    PhIndexSpan indices;
    PhBuffer gpuVertexBuffer;
    PhBuffer gpuIndexBuffer;
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