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
    PhStatus               status  = PH_SUCCESS;
    uint8_t               *code    = NULL;
    size_t                 size    = 0;
    SpvReflectShaderModule reflect = {0};
    bool                   reflected = false;

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
            .pName  = reflect.entry_points[i].name,
        };
    }

exit:
    if (reflected)
        spvReflectDestroyShaderModule(&reflect);
    PH_FREE_IF_SET(code);
    if (status != PH_SUCCESS)
    {
        if (out->vkModule)
            vkDestroyShaderModule(hDevice->device, out->vkModule, NULL);
        PH_FREE_IF_SET(out->pStages);
    }
    return status;
}

PhStatus ph_destroy_shader_module(PhDeviceHandle hDevice, PhShaderModule *module)
{
    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_NULL_CHECK(PH_LOG_ERROR, module);

    vkDestroyShaderModule(hDevice->device, module->vkModule, NULL);
    PH_FREE_IF_SET(module->pStages);
    module->vkModule    = VK_NULL_HANDLE;
    module->stageCount  = 0;
    return PH_SUCCESS;
}
