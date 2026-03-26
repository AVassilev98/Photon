#include "photon/photon_pipeline.h"
#include "photon_device_internal.h"
#include <vulkan/vulkan_core.h>
#include "photon/photon_error.h"

#include "stdlib.h"
#include "string.h"

static PhStatus _build_pipeline_layout(VkDevice device,
                                        const PhShaderModule *pShaders, uint32_t shaderCount,
                                        VkPipelineLayout *pLayout,
                                        VkDescriptorSetLayout **ppSetLayouts, uint32_t *pSetLayoutCount)
{
    PhStatus status = PH_SUCCESS;

    /* find the highest set index across all shaders */
    uint32_t maxSet     = 0;
    bool     hasBinding = false;
    for (uint32_t si = 0; si < shaderCount; si++)
    {
        for (uint32_t bi = 0; bi < pShaders[si].bindingCount; bi++)
        {
            if (pShaders[si].pBindingSets[bi] > maxSet)
                maxSet = pShaders[si].pBindingSets[bi];
            hasBinding = true;
        }
    }

    uint32_t               setCount    = hasBinding ? maxSet + 1 : 0;
    VkDescriptorSetLayout *pSetLayouts = NULL;
    uint32_t               createdSets = 0;

    if (setCount > 0)
    {
        pSetLayouts = calloc(setCount, sizeof(VkDescriptorSetLayout));
        PH_CHECK_GOTO(PH_LOG_ERROR, pSetLayouts, PH_ERR_OUT_OF_MEMORY, status, done);

        /* temp buffer large enough to hold all bindings across all shaders */
        uint32_t totalBindings = 0;
        for (uint32_t si = 0; si < shaderCount; si++)
            totalBindings += pShaders[si].bindingCount;

        VkDescriptorSetLayoutBinding *merged = malloc(sizeof(VkDescriptorSetLayoutBinding) * totalBindings);
        PH_CHECK_GOTO(PH_LOG_ERROR, merged, PH_ERR_OUT_OF_MEMORY, status, done);

        for (uint32_t set = 0; set < setCount; set++)
        {
            uint32_t mergedCount = 0;

            for (uint32_t si = 0; si < shaderCount; si++)
            {
                for (uint32_t bi = 0; bi < pShaders[si].bindingCount; bi++)
                {
                    if (pShaders[si].pBindingSets[bi] != set)
                        continue;

                    const VkDescriptorSetLayoutBinding *src = &pShaders[si].pBindings[bi];

                    /* merge stageFlags if same binding index already seen */
                    bool found = false;
                    for (uint32_t mi = 0; mi < mergedCount; mi++)
                    {
                        if (merged[mi].binding == src->binding)
                        {
                            merged[mi].stageFlags |= src->stageFlags;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        merged[mergedCount++] = *src;
                }
            }

            VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
                .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = mergedCount,
                .pBindings    = merged,
            };
            VkResult res = vkCreateDescriptorSetLayout(device, &setLayoutInfo, NULL, &pSetLayouts[set]);
            if (res != VK_SUCCESS)
            {
                free(merged);
                status = ph_status_from_vk(res);
                goto done;
            }
            createdSets++;
        }
        free(merged);
    }

    /* collect push constant ranges from all shaders */
    uint32_t totalPushRanges = 0;
    for (uint32_t si = 0; si < shaderCount; si++)
        totalPushRanges += pShaders[si].pushConstantRangeCount;

    VkPushConstantRange *pPushRanges = NULL;
    if (totalPushRanges > 0)
    {
        pPushRanges = malloc(sizeof(VkPushConstantRange) * totalPushRanges);
        PH_CHECK_GOTO(PH_LOG_ERROR, pPushRanges, PH_ERR_OUT_OF_MEMORY, status, done);
        uint32_t ri = 0;
        for (uint32_t si = 0; si < shaderCount; si++)
            for (uint32_t pi = 0; pi < pShaders[si].pushConstantRangeCount; pi++)
                pPushRanges[ri++] = pShaders[si].pPushConstantRanges[pi];
    }

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = setCount,
        .pSetLayouts            = pSetLayouts,
        .pushConstantRangeCount = totalPushRanges,
        .pPushConstantRanges    = pPushRanges,
    };
    VkResult res = vkCreatePipelineLayout(device, &layoutInfo, NULL, pLayout);
    free(pPushRanges);
    if (res != VK_SUCCESS)
    {
        status = ph_status_from_vk(res);
        goto done;
    }

    *ppSetLayouts    = pSetLayouts;
    *pSetLayoutCount = setCount;
    return PH_SUCCESS;

done:
    for (uint32_t i = 0; i < createdSets; i++)
        vkDestroyDescriptorSetLayout(device, pSetLayouts[i], NULL);
    PH_FREE_IF_SET(pSetLayouts);
    return status;
}

PhStatus ph_create_graphics_pipeline(PhDeviceHandle hDevice, PhGraphicsPipelineOptions options, PhPipeline *out)
{
    PhStatus status = PH_SUCCESS;
    size_t   totalShaderCreateInfos = 0;
    size_t   nextFreeShaderSlot     = 0;
    VkPipelineShaderStageCreateInfo *pShaderCreateInfos = NULL;
    VkPipelineLayout                 layout             = VK_NULL_HANDLE;
    VkDescriptorSetLayout           *pSetLayouts        = NULL;
    uint32_t                         setLayoutCount     = 0;
    VkPipeline                       pipeline           = VK_NULL_HANDLE;

    PH_NULL_CHECK(PH_LOG_ERROR, out);
    PH_NULL_CHECK(PH_LOG_ERROR, options.pShaders);
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, options.shaderCount > 0, PH_ERR_INVALID_ARG);

    PH_PROPAGATE_GOTO(PH_LOG_ERROR,
        _build_pipeline_layout(hDevice->device, options.pShaders, options.shaderCount,
                               &layout, &pSetLayouts, &setLayoutCount),
        status, done);

    static const VkDynamicState pDynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo pipelineDynamicState = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pDynamicStates    = pDynamicStates,
        .dynamicStateCount = PH_NUM_ELEMS(pDynamicStates),
    };

    VkPipelineViewportStateCreateInfo viewportCreateInfo = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = options.colorAttachmentCount,
        .pColorAttachmentFormats = options.pColorAttachmentFormats,
        .depthAttachmentFormat   = options.depthAttachmentFormat,
    };

    for (size_t i = 0; i < options.shaderCount; i++)
        totalShaderCreateInfos += options.pShaders[i].stageCount;

    PH_MALLOC_GOTO(PH_LOG_ERROR, pShaderCreateInfos,
        sizeof(VkPipelineShaderStageCreateInfo) * totalShaderCreateInfos, status, done);

    for (size_t i = 0; i < options.shaderCount; i++)
    {
        memcpy(pShaderCreateInfos + nextFreeShaderSlot, options.pShaders[i].pStages,
            sizeof(VkPipelineShaderStageCreateInfo) * options.pShaders[i].stageCount);
        nextFreeShaderSlot += options.pShaders[i].stageCount;
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &renderingInfo,
        .layout              = layout,
        .renderPass          = NULL,
        .pVertexInputState   = &options.inputStateInfo,
        .pInputAssemblyState = &options.inputAssemblyInfo,
        .pTessellationState  = &options.tesselationInfo,
        .pViewportState      = &viewportCreateInfo,
        .pRasterizationState = &options.rasterStateInfo,
        .pMultisampleState   = &options.multisampleStateInfo,
        .pDepthStencilState  = &options.stencilStateInfo,
        .pColorBlendState    = &(VkPipelineColorBlendStateCreateInfo){
            .sType           = options.colorBlendInfo.sType,
            .pNext           = options.colorBlendInfo.pNext,
            .flags           = options.colorBlendInfo.flags,
            .logicOpEnable   = options.colorBlendInfo.logicOpEnable,
            .logicOp         = options.colorBlendInfo.logicOp,
            .attachmentCount = options.colorAttachmentCount,
            .pAttachments    = options.pColorBlendAttachments,
        },
        .pDynamicState = &pipelineDynamicState,
        .pStages       = pShaderCreateInfos,
        .stageCount    = (uint32_t)totalShaderCreateInfos,
    };

    PH_VK_CHECK_GOTO(PH_LOG_ERROR,
        vkCreateGraphicsPipelines(hDevice->device, NULL, 1, &pipelineCreateInfo, NULL, &pipeline),
        status, done);

done:
    PH_FREE_IF_SET(pShaderCreateInfos);
    if (status == PH_SUCCESS)
    {
        out->pipeline       = pipeline;
        out->layout         = layout;
        out->pSetLayouts    = pSetLayouts;
        out->setLayoutCount = setLayoutCount;
    }
    else
    {
        if (layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(hDevice->device, layout, NULL);
        for (uint32_t i = 0; i < setLayoutCount; i++)
            vkDestroyDescriptorSetLayout(hDevice->device, pSetLayouts[i], NULL);
        PH_FREE_IF_SET(pSetLayouts);
    }
    return status;
}

PhStatus ph_destroy_pipeline(PhDeviceHandle hDevice, PhPipeline *pipeline)
{
    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_NULL_CHECK(PH_LOG_ERROR, pipeline);

    vkDestroyPipeline(hDevice->device, pipeline->pipeline, NULL);
    vkDestroyPipelineLayout(hDevice->device, pipeline->layout, NULL);
    for (uint32_t i = 0; i < pipeline->setLayoutCount; i++)
        vkDestroyDescriptorSetLayout(hDevice->device, pipeline->pSetLayouts[i], NULL);
    PH_FREE_IF_SET(pipeline->pSetLayouts);
    *pipeline = (PhPipeline){0};
    return PH_SUCCESS;
}