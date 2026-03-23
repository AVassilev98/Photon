#include "photon/photon_shader.h"
#include "photon_device_internal.h"
#include "photon/photon_error.h"

#include <stdio.h>
#include <stdlib.h>

PhStatus ph_create_shader_module(PhDeviceHandle hDevice, const char *path, VkShaderModule *out)
{
    PhStatus  status = PH_SUCCESS;
    uint8_t  *code   = NULL;

    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);
    PH_NULL_CHECK(PH_LOG_ERROR, path);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    FILE *f = fopen(path, "rb");
    PH_CHECK_OR_RETURN(PH_LOG_ERROR, f != NULL, PH_ERR_NOT_FOUND);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    code = malloc((size_t)size);
    PH_CHECK_GOTO(PH_LOG_ERROR, code != NULL, PH_ERR_OUT_OF_MEMORY, status, exit);
    PH_CHECK_GOTO(PH_LOG_ERROR, fread(code, 1, (size_t)size, f) == (size_t)size, PH_ERR_UNKNOWN, status, exit);

    VkShaderModuleCreateInfo info = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)size,
        .pCode    = (const uint32_t *)code,
    };

    PH_VK_CHECK_GOTO(PH_LOG_ERROR, vkCreateShaderModule(hDevice->device, &info, NULL, out), status, exit);

exit:
    fclose(f);
    PH_FREE_IF_SET(code);
    return status;
}

PhStatus ph_destroy_shader_module(PhDeviceHandle hDevice, VkShaderModule module)
{
    PH_NULL_CHECK(PH_LOG_ERROR, hDevice);

    vkDestroyShaderModule(hDevice->device, module, NULL);
    return PH_SUCCESS;
}
