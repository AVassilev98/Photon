#pragma once

#include "photon_instance.h"
#include "photon_status.h"
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct PhWindowSettings
{
    PhInstanceHandle hInstance;
    const char *title;
    uint32_t    width;
    uint32_t    height;
    bool        resizable;
} PhWindowSettings;

typedef struct PhExtent2D
{
    uint32_t width;
    uint32_t height;
} PhExtent2D;


typedef struct PhWindow *PhWindowHandle;
typedef VkSurfaceKHR PhSurfaceHandle;

typedef void (*PhKeyCallbackFn)(PhWindowHandle hWindow);
typedef void (*PhMouseCallbackFn)(PhWindowHandle hWindow, double xpos, double ypos);

PhStatus ph_create_window (const PhWindowSettings *settings, PhWindowHandle *out);
PhStatus ph_destroy_window(PhWindowHandle window);

bool       ph_window_should_close(PhWindowHandle window);
void       ph_window_poll_events (PhWindowHandle window);
PhExtent2D ph_window_get_extent  (PhWindowHandle window);

PhStatus ph_window_get_surface(PhWindowHandle window, PhSurfaceHandle  *out);
PhStatus ph_window_get_required_extensions(const char **out, uint32_t *count);

PhStatus ph_window_key_callback_set(PhWindowHandle window, PhKeyCallbackFn fn);
PhStatus ph_window_mouse_callback_set(PhWindowHandle window, PhMouseCallbackFn fn);
PhStatus ph_window_callback_data_set(PhWindowHandle window, void *data);
void *ph_window_callback_data_get(PhWindowHandle window);

int ph_window_key_get(PhWindowHandle window, int key);
