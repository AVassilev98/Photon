#pragma once

#include <photon/photon_log.h>
#include <photon/photon_status.h>

/*
 * PH_VK_CHECK(expr)
 *   Evaluate a VkResult. On failure, log and return the mapped PhStatus.
 */
#define PH_VK_CHECK(expr)                                                       \
    do {                                                                        \
        VkResult _vkr = (expr);                                                 \
        if (_vkr != VK_SUCCESS) {                                               \
            PH_LOG_ERROR("Vulkan error %d", _vkr);                              \
            return ph_status_from_vk(_vkr);                                     \
        }                                                                       \
    } while (0)

/*
 * PH_VK_CHECK_GOTO(expr, out, label)
 *   Like PH_VK_CHECK but jumps to label instead of returning.
 *   Writes the mapped PhStatus into out.
 */
#define PH_VK_CHECK_GOTO(expr, out, label)                                      \
    do {                                                                        \
        VkResult _vkr = (expr);                                                 \
        if (_vkr != VK_SUCCESS) {                                               \
            PH_LOG_ERROR("Vulkan error %d", _vkr);                              \
            (out) = ph_status_from_vk(_vkr);                                    \
            goto label;                                                         \
        }                                                                       \
    } while (0)

/*
 * PH_CHECK(expr)
 *   Evaluate a PhStatus expression. On failure, propagate by returning it.
 */
#define PH_CHECK(expr)                                                          \
    do {                                                                        \
        PhStatus _s = (expr);                                                   \
        if (_s != PH_SUCCESS) return _s;                                        \
    } while (0)

/*
 * PH_CHECK_GOTO(cond, err, out, label)
 *   Jump to label if cond is false. Writes err into out.
 *   cond can be any expression: pointer, bool, integer, comparison, etc.
 *
 *   void *buf = malloc(n);
 *   PH_CHECK_GOTO(buf, PH_ERR_OUT_OF_MEMORY, status, cleanup);
 *
 *   PH_CHECK_GOTO(write(fd, data, len) == len, PH_ERR_UNKNOWN, status, cleanup);
 */
#define PH_CHECK_GOTO(cond, err, out, label)                                    \
    do {                                                                        \
        if (!(cond)) {                                                          \
            (out) = (err);                                                      \
            goto label;                                                         \
        }                                                                       \
    } while (0)

/*
 * PH_PROPAGATE_GOTO(expr, out, label)
 *   Evaluate a PhStatus expression. If it fails, write the status into out
 *   and jump to label.
 *
 *   PH_PROPAGATE_GOTO(ph_create_device(&dev), status, cleanup);
 */
#define PH_PROPAGATE_GOTO(expr, out, label)                                     \
    do {                                                                        \
        PhStatus _s = (expr);                                                   \
        if (_s != PH_SUCCESS) {                                                 \
            (out) = _s;                                                         \
            goto label;                                                         \
        }                                                                       \
    } while (0)

/*
 * PH_NULL_CHECK(ptr)
 *   Return PH_ERR_INVALID_ARG if ptr is NULL.
 */
#define PH_NULL_CHECK(ptr)                                                      \
    do {                                                                        \
        if (!(ptr)) return PH_ERR_INVALID_ARG;                                  \
    } while (0)

/*
 * PH_NULL_CHECK_GOTO(ptr, out, label)
 *   Jump to label if ptr is NULL. Writes PH_ERR_INVALID_ARG into out.
 */
#define PH_NULL_CHECK_GOTO(ptr, out, label)                                     \
    do {                                                                        \
        if (!(ptr)) {                                                           \
            (out) = PH_ERR_INVALID_ARG;                                         \
            goto label;                                                         \
        }                                                                       \
    } while (0)

#define PH_NUM_ELEMS(arr)                                                       \
    (sizeof(arr) / sizeof(*arr))

#define PH_FREE_IF_SET(ptr)                                                     \
    do {                                                                        \
        if ((ptr) != NULL) free(ptr);                                           \
    } while (0)
