#pragma once

#include "photon_status.h"
#include "stdbool.h"

typedef struct PhInstanceSettings
{
    const char *appName;
    uint32_t    appVersion;
    bool enableDebug;
} PhInstanceSettings;

typedef struct PhInstance *PhInstanceHandle;
PhStatus ph_create_instance(PhInstanceSettings *settings, PhInstanceHandle *out);
PhStatus ph_destroy_instance(PhInstanceHandle handle);
