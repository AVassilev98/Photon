
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "photon/photon.h"
#include "photon/photon_error.h"
#include "photon/photon_device.h"

#include "stdio.h"

int main(void) {
    PhInstanceHandle hInstance;
    PhWindowHandle hWindow;

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

    PhCapability deviceCaps = {
        .asyncComputeQueue = true,
        .dedicatedTransfer = true,
        .swapchain = true,
        .graphicsQueue = true,
        .discrete = true,
        .rtCapable = true,
        .minimumImageDimensions = {
            .height = 1080,
            .width = 1920,
        },
        .timelineSemaphore = true,
        .synchronization2 = true,
        .descriptorIndexing = true,
    };

    PhDeviceInfoSpan deviceInfos;
    PH_CHECK(PH_LOG_ERROR, ph_enumerate_devices(hInstance, deviceCaps, &deviceInfos));


    PhPresentOptions presentOptions = {
        .format = {
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        },
        .mode = VK_PRESENT_MODE_FIFO_KHR
    };
    PhSurfaceHandle hSurface;
    PH_CHECK(PH_LOG_ERROR, ph_window_get_surface(hWindow, &hSurface));
    PH_CHECK(PH_LOG_ERROR, ph_configure_device_for_present(deviceInfos.ptr[0].handle, hSurface, presentOptions));


    while(!ph_window_should_close(hWindow))
    {
        ph_window_poll_events(hWindow);
    }

    return 0;
}
