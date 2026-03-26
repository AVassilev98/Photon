#include "photon/photon_pipeline.h"
#include "photon_device_internal.h"
#include <vulkan/vulkan_core.h>
#include "photon/photon_error.h"

#include "stdlib.h"
#include "string.h"

PhStatus ph_create_graphics_pipeline(PhDeviceHandle hDevice, PhGraphicsPipelineOptions options, VkPipeline *out)
{
    PhStatus status = PH_SUCCESS;
    size_t totalShaderCreateInfos = 0;
    size_t nextFreeShaderSlot = 0;
    VkPipelineShaderStageCreateInfo *pShaderCreateInfos = NULL;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    PH_NULL_CHECK(PH_LOG_ERROR, out);
    PH_NULL_CHECK(PH_LOG_ERROR, options.pShaders);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, options.shaderCount > 0, PH_ERR_INVALID_ARG);

    static const VkDynamicState pDynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo pipelineDynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pDynamicStates = pDynamicStates,
        .dynamicStateCount = PH_NUM_ELEMS(pDynamicStates)
    };

    VkPipelineViewportStateCreateInfo viewportCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pViewports = NULL,
        .viewportCount = 1,
        .pScissors = NULL,
        .scissorCount = 1,
    };

    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = options.colorAttachmentCount,
        .pColorAttachmentFormats = options.pColorAttachmentFormats,
        .depthAttachmentFormat   = options.depthAttachmentFormat,
    };

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0,
        .pushConstantRangeCount = 0,
    };
    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreatePipelineLayout(hDevice->device, &layoutInfo, NULL, &layout), status, done);

    for (size_t i = 0; i < options.shaderCount; i++)
    {
        totalShaderCreateInfos += options.pShaders[i].stageCount;
    }
    
    PH_MALLOC_GOTO(PH_LOG_ERROR, pShaderCreateInfos, sizeof(VkPipelineShaderStageCreateInfo) * totalShaderCreateInfos, status, done);
    for (size_t i = 0; i < options.shaderCount; i++)
    {
        memcpy(pShaderCreateInfos + nextFreeShaderSlot, options.pShaders[i].pStages,
            sizeof(VkPipelineShaderStageCreateInfo) * options.pShaders[i].stageCount);
        nextFreeShaderSlot += options.pShaders[i].stageCount;
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .layout = layout,
        .renderPass = NULL,
        .pVertexInputState = &options.inputStateInfo,
        .pInputAssemblyState = &options.inputAssemblyInfo,
        .pTessellationState = &options.tesselationInfo,
        .pViewportState = &viewportCreateInfo,
        .pRasterizationState = &options.rasterStateInfo,
        .pMultisampleState = &options.multisampleStateInfo,
        .pDepthStencilState = &options.stencilStateInfo,
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
            .sType           = options.colorBlendInfo.sType,
            .pNext           = options.colorBlendInfo.pNext,
            .flags           = options.colorBlendInfo.flags,
            .logicOpEnable   = options.colorBlendInfo.logicOpEnable,
            .logicOp         = options.colorBlendInfo.logicOp,
            .attachmentCount = options.colorAttachmentCount,
            .pAttachments    = options.pColorBlendAttachments,
        },
        .pDynamicState = &pipelineDynamicState,
        .pNext = &renderingInfo,
        .pStages = pShaderCreateInfos,
        .stageCount = totalShaderCreateInfos,
    };

    PH_VK_CHECK_GOTO(PH_LOG_ERROR, 
        vkCreateGraphicsPipelines(hDevice->device, NULL, 1, &pipelineCreateInfo, NULL, &pipeline), status, done);

done:
    PH_FREE_IF_SET(pShaderCreateInfos);
    if (status == PH_SUCCESS)
    {
        *out = pipeline;
    }
    return status;
}