#pragma once

#include "photon_instance.h"
#include "photon_window.h"
#include "foundation/span.h"

typedef struct PhDevice *PhDeviceHandle;

typedef struct PhCapability {
    uint32_t discrete               : 1;
    uint32_t rtCapable              : 1;
    uint32_t swapchain              : 1;
    uint32_t graphicsQueue          : 1;
    uint32_t asyncComputeQueue      : 1;
    uint32_t dedicatedTransfer      : 1;
    uint32_t samplerAnisotropy      : 1;
    uint32_t fillModeNonSolid       : 1;
    uint32_t wideLines              : 1;
    uint32_t largePoints            : 1;
    uint32_t multiDrawIndirect      : 1;
    uint32_t shaderInt64            : 1;
    uint32_t shaderFloat64          : 1;
    uint32_t bufferDeviceAddress    : 1;
    uint32_t descriptorIndexing     : 1;
    uint32_t timelineSemaphore      : 1;
    uint32_t dynamicRendering       : 1;
    uint32_t synchronization2       : 1;
    PhExtent2D minimumImageDimensions;
    uint32_t   minPushConstantsSize;
    uint64_t   minimumVRAM;
} PhCapability;

typedef struct PhDeviceInfo {
    char *pName;
    PhDeviceHandle handle;
    PhCapability capabilities;
} PhDeviceInfo;
FDN_SPAN_DEFINE(PhDeviceInfo, PhDeviceInfoSpan)


PhStatus ph_enumerate_devices(PhInstanceHandle hInstance, PhCapability caps, PhDeviceInfoSpan *ppDeviceInfo);