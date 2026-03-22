
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "photon/photon.h"
#include "stdio.h"

int main(void) {
    PhInstanceHandle handle;
    PhInstanceSettings instanceSettings = {
        .appName = "Photon",
        .appVersion = 1,
        .enableDebug = true,
    };

    ph_create_instance(&instanceSettings, &handle);
    return 0;
}
