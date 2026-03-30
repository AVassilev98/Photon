#include "photon/photon_window.h"
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "photon/photon_log.h"
#include "photon/photon_status.h"
#include "photon_window_internal.h"
#include "photon_instance_internal.h"
#include "photon/photon_error.h"

#include <stdlib.h>

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
    glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    PhWindow *window = calloc(1, sizeof(PhWindow));
    PH_CHECK_GOTO(PH_LOG_ERROR, window != NULL, PH_ERR_OUT_OF_MEMORY, status, cleanup);

    *window = (PhWindow) {
        .cursorActive = false,
        .escLastPressedTimestamp = 0.0f,
        .glfwWindow = glfwWindow,
        .hInstance = settings->hInstance,
    };
    glfwSetWindowUserPointer(glfwWindow, window);

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

    double timestamp = glfwGetTime();

    if (glfwGetKey(window->glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS && (timestamp - window->escLastPressedTimestamp) > 0.01)
    {
        if (window->cursorActive)
        {
            window->cursorActive = false;
            glfwSetInputMode(window->glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else
        {
            window->cursorActive = true;
            glfwSetInputMode(window->glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        window->escLastPressedTimestamp = timestamp;
    }

    if (window->cursorActive)
    {
        return;
    }
    if (window->pKeyCallback)
    {
        window->pKeyCallback(window);
    }
    if (window->pMouseCallback)
    {
        double xpos;
        double ypos;
        glfwGetCursorPos(window->glfwWindow, &xpos, &ypos);
        window->pMouseCallback(window, xpos, ypos);
    }
}

int ph_window_key_get(PhWindowHandle window, int key)
{
    return glfwGetKey(window->glfwWindow, key);
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

PhStatus ph_window_key_callback_set(PhWindowHandle window, PhKeyCallbackFn fn)
{
    PH_NULL_CHECK(PH_LOG_ERROR, window);
    PH_NULL_CHECK(PH_LOG_ERROR, fn);

    window->pKeyCallback = fn;
    return PH_SUCCESS;
}

PhStatus ph_window_mouse_callback_set(PhWindowHandle window, PhMouseCallbackFn fn)
{
    PH_NULL_CHECK(PH_LOG_ERROR, window);
    PH_NULL_CHECK(PH_LOG_ERROR, fn);

    window->pMouseCallback = fn;
    return PH_SUCCESS;
}

PhStatus ph_window_callback_data_set(PhWindowHandle window, void *data)
{
    PH_NULL_CHECK(PH_LOG_ERROR, window);
    PH_NULL_CHECK(PH_LOG_ERROR, data);

    window->pCallbackData = data;
    return PH_SUCCESS;
}

void *ph_window_callback_data_get(PhWindowHandle window)
{
    return window->pCallbackData;
}