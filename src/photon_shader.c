#include "photon/photon_shader.h"
#include "photon_device_internal.h"
#include "photon/photon_error.h"

#include <spirv_reflect.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PhStatus _load_spirv(const char *path, uint8_t **ppCode, size_t *pSize)
{
    PhStatus  status = PH_SUCCESS;
    uint8_t  *code   = NULL;

    FILE *f = fopen(path, "rb");
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, f != NULL, PH_ERR_NOT_FOUND);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    PH_MALLOC_GOTO(PH_LOG_ERROR, code, (size_t)size, status, exit);
    PH_CHECK_GOTO(PH_LOG_ERROR, fread(code, 1, (size_t)size, f) == (size_t)size, PH_ERR_UNKNOWN, status, exit);

    *ppCode = code;
    *pSize  = (size_t)size;

exit:
    fclose(f);
    if (status != PH_SUCCESS)
        PH_FREE_IF_SET(code);
    return status;
}

PhStatus ph_create_shader_module(PhDeviceHandle hDevice, const char *path, PhShaderModule *out)
{
    PhStatus                      status       = PH_SUCCESS;
    uint8_t                      *code         = NULL;
    size_t                        size         = 0;
    SpvReflectShaderModule        reflect      = {0};
    bool                          reflected    = false;
    SpvReflectDescriptorBinding **ppSpvBindings = NULL;
    SpvReflectBlockVariable     **ppPushBlocks  = NULL;

    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_NULL_CHECK(PH_LOG_ERROR, path);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    PH_PROPAGATE_GOTO(PH_LOG_ERROR, _load_spirv(path, &code, &size), status, exit);

    PH_CHECK_GOTO(PH_LOG_ERROR,
        spvReflectCreateShaderModule(size, code, &reflect) == SPV_REFLECT_RESULT_SUCCESS,
        PH_ERR_SHADER_COMPILE, status, exit);
    reflected = true;

    VkShaderModuleCreateInfo moduleInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode    = (const uint32_t *)code,
    };
    PH_VK_CHECK_GOTO(PH_LOG_ERROR,
        vkCreateShaderModule(hDevice->device, &moduleInfo, NULL, &out->vkModule),
        status, exit);

    out->stageCount = reflect.entry_point_count;
    PH_MALLOC_GOTO(PH_LOG_ERROR, out->pStages,
        sizeof(VkPipelineShaderStageCreateInfo) * out->stageCount, status, exit);

    for (uint32_t i = 0; i < out->stageCount; i++)
    {
        out->pStages[i] = (VkPipelineShaderStageCreateInfo) {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = (VkShaderStageFlagBits)reflect.entry_points[i].shader_stage,
            .module = out->vkModule,
            .pName  = strdup(reflect.entry_points[i].name),
        };
    }

    VkShaderStageFlags moduleStageFlags = 0;
    for (uint32_t i = 0; i < reflect.entry_point_count; i++)
        moduleStageFlags |= (VkShaderStageFlags)reflect.entry_points[i].shader_stage;

    uint32_t bindingCount = 0;
    spvReflectEnumerateDescriptorBindings(&reflect, &bindingCount, NULL);
    if (bindingCount > 0)
    {
        ppSpvBindings = malloc(sizeof(SpvReflectDescriptorBinding *) * bindingCount);
        PH_CHECK_GOTO(PH_LOG_ERROR, ppSpvBindings, PH_ERR_OUT_OF_MEMORY, status, exit);
        spvReflectEnumerateDescriptorBindings(&reflect, &bindingCount, ppSpvBindings);

        PH_MALLOC_GOTO(PH_LOG_ERROR, out->pBindings,    sizeof(VkDescriptorSetLayoutBinding) * bindingCount, status, exit);
        PH_MALLOC_GOTO(PH_LOG_ERROR, out->pBindingSets, sizeof(uint32_t) * bindingCount,                     status, exit);
        out->bindingCount = bindingCount;

        for (uint32_t i = 0; i < bindingCount; i++)
        {
            out->pBindingSets[i] = ppSpvBindings[i]->set;
            out->pBindings[i]   = (VkDescriptorSetLayoutBinding){
                .binding         = ppSpvBindings[i]->binding,
                .descriptorType  = (VkDescriptorType)ppSpvBindings[i]->descriptor_type,
                .descriptorCount = ppSpvBindings[i]->count,
                .stageFlags      = moduleStageFlags,
            };
        }
    }

    uint32_t pushCount = 0;
    spvReflectEnumeratePushConstantBlocks(&reflect, &pushCount, NULL);
    if (pushCount > 0)
    {
        ppPushBlocks = malloc(sizeof(SpvReflectBlockVariable *) * pushCount);
        PH_CHECK_GOTO(PH_LOG_ERROR, ppPushBlocks, PH_ERR_OUT_OF_MEMORY, status, exit);
        spvReflectEnumeratePushConstantBlocks(&reflect, &pushCount, ppPushBlocks);

        PH_MALLOC_GOTO(PH_LOG_ERROR, out->pPushConstantRanges, sizeof(VkPushConstantRange) * pushCount, status, exit);
        out->pushConstantRangeCount = pushCount;

        for (uint32_t i = 0; i < pushCount; i++)
        {
            out->pPushConstantRanges[i] = (VkPushConstantRange){
                .stageFlags = moduleStageFlags,
                .offset     = ppPushBlocks[i]->offset,
                .size       = ppPushBlocks[i]->size,
            };
        }
    }

exit:
    PH_FREE_IF_SET(ppSpvBindings);
    PH_FREE_IF_SET(ppPushBlocks);
    if (reflected)
        spvReflectDestroyShaderModule(&reflect);
    PH_FREE_IF_SET(code);
    if (status != PH_SUCCESS)
    {
        if (out->vkModule)
            vkDestroyShaderModule(hDevice->device, out->vkModule, NULL);
        PH_FREE_IF_SET(out->pStages);
        PH_FREE_IF_SET(out->pBindings);
        PH_FREE_IF_SET(out->pBindingSets);
        PH_FREE_IF_SET(out->pPushConstantRanges);
    }
    return status;
}

PhStatus ph_destroy_shader_module(PhDeviceHandle hDevice, PhShaderModule *module)
{
    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_NULL_CHECK(PH_LOG_ERROR, module);

    vkDestroyShaderModule(hDevice->device, module->vkModule, NULL);
    for (uint32_t i = 0; i < module->stageCount; i++)
        PH_FREE_IF_SET((void *)module->pStages[i].pName);
    PH_FREE_IF_SET(module->pStages);
    PH_FREE_IF_SET(module->pBindings);
    PH_FREE_IF_SET(module->pBindingSets);
    PH_FREE_IF_SET(module->pPushConstantRanges);
    *module = (PhShaderModule){0};
    return PH_SUCCESS;
}
