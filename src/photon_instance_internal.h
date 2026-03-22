#pragma once

#include <vulkan/vulkan.h>
#include "photon/photon_instance.h"

struct PhInstance
{
    VkInstance               instance;
    VkDebugUtilsMessengerEXT debugMessenger;
};

typedef struct PhInstance PhInstance;
