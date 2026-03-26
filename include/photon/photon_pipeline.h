#pragma once

#include "photon/photon_device.h"
#include "photon/photon_shader.h"
#include <vulkan/vulkan_core.h>

typedef VkPipelineVertexInputStateCreateInfo    PhVertexInputStateOptions;
typedef VkPipelineInputAssemblyStateCreateInfo  PhInputAssemblyOptions;
typedef VkPipelineTessellationStateCreateInfo   PhTesselationOptions;
typedef VkPipelineRasterizationStateCreateInfo  PhRasterizationOptions;
typedef VkPipelineMultisampleStateCreateInfo    PhMultiSampleOptions;
typedef VkPipelineDepthStencilStateCreateInfo   PhDepthStencilOptions;
typedef VkPipelineColorBlendStateCreateInfo     PhColorBlendOptions;
typedef VkPipelineColorBlendAttachmentState     PhColorBlendAttachmentOptions;

typedef struct PhGraphicsPipelineOptions
{
    PhVertexInputStateOptions            inputStateInfo;
    PhInputAssemblyOptions               inputAssemblyInfo;
    PhTesselationOptions                 tesselationInfo;
    PhRasterizationOptions               rasterStateInfo;
    PhMultiSampleOptions                 multisampleStateInfo;
    PhDepthStencilOptions                stencilStateInfo;
    PhColorBlendOptions                  colorBlendInfo;
    VkPipelineColorBlendAttachmentState *pColorBlendAttachments;
    PhShaderModule                      *pShaders;
    uint32_t                             shaderCount;
    VkFormat                            *pColorAttachmentFormats;
    uint32_t                             colorAttachmentCount;
    VkFormat                             depthAttachmentFormat;
} PhGraphicsPipelineOptions;

#define PH_INPUT_STATE_OPTIONS_DEFAULT (PhVertexInputStateOptions) {                        \
    .sType      = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,                \
}

#define PH_INPUT_ASSEMBLY_OPTIONS_DEFAULT (PhInputAssemblyOptions) {                        \
    .sType      = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,              \
    .pNext      = NULL,                                                                     \
    .flags      = 0UL,                                                                      \
    .topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                                      \
    .primitiveRestartEnable = VK_FALSE                                                      \
}

#define PH_TESSELATION_OPTIONS_DEFAULT (PhTesselationOptions) {                             \
    .sType                   = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,   \
    .pNext                   = NULL,                                                        \
    .flags                   = 0UL,                                                         \
    .patchControlPoints      = 0UL,                                                         \
}

#define PH_RASTERIZATION_OPTIONS_DEFAULT (PhRasterizationOptions) {                         \
    .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  \
    .depthClampEnable        = VK_FALSE,                                                    \
    .rasterizerDiscardEnable = VK_FALSE,                                                    \
    .polygonMode             = VK_POLYGON_MODE_FILL,                                        \
    .cullMode                = VK_CULL_MODE_BACK_BIT,                                       \
    .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,                             \
    .depthBiasEnable         = VK_FALSE,                                                    \
    .lineWidth               = 1.0f,                                                        \
}

#define PH_MULTI_SAMPLE_OPTIONS_DEFAULT (PhMultiSampleOptions) {                            \
    .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,       \
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,                                          \
    .sampleShadingEnable  = VK_FALSE,                                                       \
    .minSampleShading     = 1.0f,                                                           \
    .pSampleMask          = NULL,                                                           \
    .alphaToCoverageEnable = VK_FALSE,                                                      \
    .alphaToOneEnable     = VK_FALSE,                                                       \
}

#define PH_DEPTH_STENCIL_OPTIONS_DEFAULT (PhDepthStencilOptions) {                          \
    .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,    \
    .depthTestEnable       = VK_TRUE,                                                       \
    .depthWriteEnable      = VK_TRUE,                                                       \
    .depthCompareOp        = VK_COMPARE_OP_LESS,                                            \
    .depthBoundsTestEnable = VK_FALSE,                                                      \
    .stencilTestEnable     = VK_FALSE,                                                      \
    .minDepthBounds        = 0.0f,                                                          \
    .maxDepthBounds        = 1.0f,                                                          \
}

#define PH_COLOR_BLEND_ATTACHMENT_OPTIONS_DEFAULT (PhColorBlendAttachmentOptions) {         \
    .blendEnable = VK_FALSE,                                                                \
}

#define PH_COLOR_BLEND_OPTIONS_DEFAULT (PhColorBlendOptions) {                              \
    .sType                 = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,      \
    .pNext                 = NULL,                                                          \
    .flags                 = 0UL,                                                           \
    .logicOpEnable         = VK_FALSE,                                                      \
    .logicOp               = VK_LOGIC_OP_CLEAR,                                             \
}

#define PH_PIPELINE_OPTIONS_DEFAULT (PhGraphicsPipelineOptions) {                           \
    .inputStateInfo         = PH_INPUT_STATE_OPTIONS_DEFAULT,                               \
    .inputAssemblyInfo      = PH_INPUT_ASSEMBLY_OPTIONS_DEFAULT,                            \
    .tesselationInfo        = PH_TESSELATION_OPTIONS_DEFAULT,                               \
    .rasterStateInfo        = PH_RASTERIZATION_OPTIONS_DEFAULT,                             \
    .multisampleStateInfo   = PH_MULTI_SAMPLE_OPTIONS_DEFAULT,                              \
    .stencilStateInfo       = PH_DEPTH_STENCIL_OPTIONS_DEFAULT,                             \
    .pShaders               = NULL,                                                         \
    .shaderCount            = 0UL                                                           \
}




PhStatus ph_create_graphics_pipeline(PhDeviceHandle hDevice, PhGraphicsPipelineOptions options, VkPipeline *out);
