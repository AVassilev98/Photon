#pragma once

#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "photon/photon_window.h"

struct PhWindow
{
    GLFWwindow          *glfwWindow;
    VkSurfaceKHR        surface;
    PhInstanceHandle    hInstance;
    
    void                *pCallbackData;
    PhKeyCallbackFn     pKeyCallback;
    PhMouseCallbackFn   pMouseCallback;

    bool                cursorActive;
    double              escLastPressedTimestamp;
};

typedef struct PhWindow PhWindow;
