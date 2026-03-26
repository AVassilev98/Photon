#pragma once

#include "photon_status.h"
#include "photon_device.h"
#include <vulkan/vulkan.h>

typedef struct PhShaderModule
{
    VkShaderModule                   vkModule;
    VkPipelineShaderStageCreateInfo *pStages;
    uint32_t                         stageCount;
    uint32_t                        *pBindingSets;
    VkDescriptorSetLayoutBinding    *pBindings;
    uint32_t                         bindingCount;
    VkPushConstantRange             *pPushConstantRanges;
    uint32_t                         pushConstantRangeCount;
} PhShaderModule;

PhStatus ph_create_shader_module (PhDeviceHandle hDevice, const char *path, PhShaderModule *out);
PhStatus ph_destroy_shader_module(PhDeviceHandle hDevice, PhShaderModule *module);
