
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "photon/photon.h"

#include "stdio.h"

int main(void) {
    PhInstanceHandle hInstance                  = { 0 };
    PhWindowHandle hWindow                      = { 0 };
    PhCapability deviceCaps                     = { 0 };
    PhDeviceInfoSpan deviceInfos                = { 0 };
    PhPresentOptions presentOptions             = { 0 };
    PhSurfaceHandle hSurface                    = { 0 };
    PhDeviceHandle chosenDevice                 = { 0 };
    PhShaderModule triangleShader               = { 0 };
    PhGraphicsPipelineOptions pipelineOptions   = PH_PIPELINE_OPTIONS_DEFAULT;
    PhPipeline pipeline                         = { 0 };

    PhInstanceSettings instanceSettings = {
        .appName = "Photon",
        .appVersion = 1,
        .enableDebug = true,
    };
    PH_CHECK(PH_LOG_ERROR, ph_create_instance(&instanceSettings, &hInstance));

    PhWindowSettings windowSettings = {
        .height = 1080,
        .width = 1920,
        .resizable = true,
        .title = "Photon",
        .hInstance = hInstance
    };
    PH_CHECK(PH_LOG_ERROR, ph_create_window(&windowSettings, &hWindow));

    deviceCaps = (PhCapability) {
#ifndef __APPLE__
        .asyncComputeQueue = true,
        .dedicatedTransfer = true,
        .rtCapable = true,
        .discrete = true,
#endif
        .swapchain = true,
        .graphicsQueue = true,
        .minimumImageDimensions = {
            .height = 1080,
            .width = 1920,
        },
        .timelineSemaphore = true,
        .synchronization2 = true,
        .descriptorIndexing = true,
        .dynamicRendering = true,
    };

    PH_CHECK(PH_LOG_ERROR, ph_enumerate_devices(hInstance, deviceCaps, &deviceInfos));

    presentOptions = (PhPresentOptions) {
        .format = {
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        },
        .mode = VK_PRESENT_MODE_FIFO_KHR
    };
    PH_CHECK(PH_LOG_ERROR, ph_window_get_surface(hWindow, &hSurface));
    chosenDevice = deviceInfos.ptr[0].handle;
    PH_CHECK(PH_LOG_ERROR, ph_configure_device_for_present(chosenDevice, hSurface, presentOptions));
    ph_create_shader_module(deviceInfos.ptr[0].handle, SHADER_DIR "/triangle.spv", &triangleShader);

    pipelineOptions.pShaders    = &triangleShader;
    pipelineOptions.shaderCount = 1UL;
    ph_create_graphics_pipeline(chosenDevice, pipelineOptions, &pipeline);

    while(!ph_window_should_close(hWindow))
    {
        ph_window_poll_events(hWindow);
    }

    return 0;
}
