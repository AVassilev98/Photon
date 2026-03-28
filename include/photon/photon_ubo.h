#pragma once

#include "photon/photon_device.h"

typedef struct PhUBOCreateInfo {
    uint32_t size;
} PhUBOCreateInfo;

typedef struct PhUBO {
    PhBuffer ubo;
} PhUBO;

typedef struct PhUBOPerFrame {
    PhPerFrameResourceHandle uboDataHandle;
} PhUBOPerFrame;

PhStatus ph_ubo_create(PhDeviceHandle hDevice, PhUBOCreateInfo *pParams, PhUBO *pOut);
PhStatus ph_ubo_destroy(PhDeviceHandle hDevice, PhUBO *pUbo);

PhStatus ph_ubo_per_frame_create(PhDeviceHandle hDevice, PhUBOCreateInfo *pParams, PhUBOPerFrame *pOut);
PhStatus ph_ubo_per_frame_get(PhDeviceHandle hDevice, PhUBOPerFrame *pUboPerFrame, PhUBO **ppOut);
PhStatus ph_ubo_per_frame_destroy(PhDeviceHandle hDevice, PhUBOPerFrame *pUboPerFrame);