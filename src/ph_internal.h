#pragma once

#include <photon/result.h>
#include <stdio.h>

/* Evaluate a VkResult; on failure store it and return PH_ERROR_VULKAN. */
#define PH_VK_CHECK(expr)                                                   \
    do {                                                                    \
        VkResult _r = (expr);                                               \
        if (_r != VK_SUCCESS) {                                             \
            fprintf(stderr, "[photon] Vulkan error %d at %s:%d\n",         \
                    _r, __FILE__, __LINE__);                                \
            return PH_ERROR_VULKAN;                                         \
        }                                                                   \
    } while (0)

#define PH_CHECK(expr)                                                      \
    do {                                                                    \
        PhResult _pr = (expr);                                              \
        if (_pr != PH_SUCCESS) return _pr;                                  \
    } while (0)

#define PH_NULL_CHECK(ptr)                                                  \
    do {                                                                    \
        if (!(ptr)) return PH_ERROR_INVALID_ARG;                           \
    } while (0)
