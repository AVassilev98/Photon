#include "photon_window_internal.h"
#include "photon_instance_internal.h"
#include "photon/photon_error.h"

#include <stdlib.h>

/* ---- GLFW callbacks ------------------------------------------------------ */

static void framebufferResizeCallback(GLFWwindow *glfwWindow, int width, int height)
{
    (void)width;
    (void)height;
    (void)glfwWindow;
    /* Resize events are detected by checking ph_window_get_extent() each frame.
       Extend this callback if you need an explicit resize notification.        */
}

/* ---- Public API ---------------------------------------------------------- */

PhStatus ph_create_window(const PhWindowSettings *settings, PhWindowHandle *out)
{
    PhStatus status = PH_SUCCESS;

    PH_NULL_CHECK(PH_LOG_ERROR, settings);
    PH_NULL_CHECK(PH_LOG_ERROR, settings->title);
    PH_NULL_CHECK(PH_LOG_ERROR, settings->hInstance);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    int bInit = glfwInit();
    PH_CHECK_GOTO(PH_LOG_ERROR, bInit == GLFW_TRUE, PH_ERR_UNKNOWN, status, cleanup);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE,  settings->resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow *glfwWindow = glfwCreateWindow(
        (int)settings->width, (int)settings->height,
        settings->title, NULL, NULL);
    PH_CHECK_GOTO(PH_LOG_ERROR, glfwWindow != NULL, PH_ERR_UNKNOWN, status, cleanup);

    PhWindow *window = calloc(1, sizeof(PhWindow));
    PH_CHECK_GOTO(PH_LOG_ERROR, window != NULL, PH_ERR_OUT_OF_MEMORY, status, cleanup);

    window->glfwWindow = glfwWindow;
    window->hInstance = settings->hInstance;
    glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);

    *out = window;

cleanup:
    if (status != PH_SUCCESS)
    {
        PH_FREE_IF_SET(window);
        if (glfwWindow)
        {
            glfwDestroyWindow(glfwWindow);
        }
        if (bInit)
        {
            glfwTerminate();
        }
        return status;
    }

    return PH_SUCCESS;
}

PhStatus ph_destroy_window(PhWindowHandle window)
{
    PH_NULL_CHECK(PH_LOG_ERROR, window);

    if (window->surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(window->hInstance->instance, window->surface, NULL);

    glfwDestroyWindow(window->glfwWindow);
    glfwTerminate();
    free(window);
    return PH_SUCCESS;
}

bool ph_window_should_close(PhWindowHandle window)
{
    return glfwWindowShouldClose(window->glfwWindow);
}

void ph_window_poll_events(PhWindowHandle window)
{
    (void)window;
    glfwPollEvents();
}

PhExtent2D ph_window_get_extent(PhWindowHandle window)
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(window->glfwWindow, &w, &h);
    return (PhExtent2D){ (uint32_t)w, (uint32_t)h };
}

PhStatus ph_window_get_surface(PhWindowHandle window, PhSurfaceHandle *out)
{
    PH_NULL_CHECK(PH_LOG_ERROR, window);
    PH_NULL_CHECK(PH_LOG_ERROR, out);

    if (window->surface == VK_NULL_HANDLE)
    {
        VkResult result = glfwCreateWindowSurface(window->hInstance->instance,
                                                  window->glfwWindow,
                                                  NULL,
                                                  &window->surface);
        if (result != VK_SUCCESS)
            return ph_status_from_vk(result);
    }

    *out = window->surface;
    return PH_SUCCESS;
}

PhStatus ph_window_get_required_extensions(const char **out, uint32_t *count)
{
    PH_NULL_CHECK(PH_LOG_ERROR, count);

    if (!glfwInit())
        return PH_ERR_UNKNOWN;

    uint32_t     glfwCount = 0;
    const char **glfwExts  = glfwGetRequiredInstanceExtensions(&glfwCount);

    if (!glfwExts)
        return PH_ERR_UNSUPPORTED;

    *count = glfwCount;

    if (out)
        for (uint32_t i = 0; i < glfwCount; i++)
            out[i] = glfwExts[i];

    return PH_SUCCESS;
}
