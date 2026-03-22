#pragma once

#include <photon/photon_log.h>
#include <photon/photon_status.h>

/*
 * PH_VK_CHECK(level, expr)
 *   Evaluate a VkResult. On failure, log at level and return the mapped PhStatus.
 */
#define PH_VK_CHECK(level, expr)                                                \
    do {                                                                        \
        VkResult _vkr = (expr);                                                 \
        if (_vkr != VK_SUCCESS) {                                               \
            ph_log(level, __FILE__, __LINE__,                                   \
                   "Vulkan error %d: %s", _vkr,                                 \
                   ph_status_str(ph_status_from_vk(_vkr)));                     \
            return ph_status_from_vk(_vkr);                                     \
        }                                                                       \
    } while (0)

/*
 * PH_VK_CHECK_GOTO(level, expr, out, label)
 *   Like PH_VK_CHECK but jumps to label instead of returning.
 *   Writes the mapped PhStatus into out.
 */
#define PH_VK_CHECK_GOTO(level, expr, out, label)                               \
    do {                                                                        \
        VkResult _vkr = (expr);                                                 \
        if (_vkr != VK_SUCCESS) {                                               \
            ph_log(level, __FILE__, __LINE__,                                   \
                   "Vulkan error %d: %s", _vkr,                                 \
                   ph_status_str(ph_status_from_vk(_vkr)));                     \
            (out) = ph_status_from_vk(_vkr);                                    \
            goto label;                                                         \
        }                                                                       \
    } while (0)

/*
 * PH_CHECK(level, expr)
 *   Evaluate a PhStatus expression. On failure, log at level and return it.
 */
#define PH_CHECK(level, expr)                                                   \
    do {                                                                        \
        PhStatus _s = (expr);                                                   \
        if (_s != PH_SUCCESS) {                                                 \
            ph_log(level, __FILE__, __LINE__,                                   \
                   "Status error: %s", ph_status_str(_s));                      \
            return _s;                                                          \
        }                                                                       \
    } while (0)

/*
 * PH_CHECK_GOTO(level, cond, err, out, label)
 *   Jump to label if cond is false. Logs at level and writes err into out.
 *   cond can be any expression: pointer, bool, integer, comparison, etc.
 *
 *   void *buf = malloc(n);
 *   PH_CHECK_GOTO(PH_LOG_ERROR, buf, PH_ERR_OUT_OF_MEMORY, status, cleanup);
 *
 *   PH_CHECK_GOTO(PH_LOG_ERROR, write(fd, data, len) == len, PH_ERR_UNKNOWN, status, cleanup);
 */
#define PH_CHECK_GOTO(level, cond, err, out, label)                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ph_log(level, __FILE__, __LINE__,                                   \
                   "Check failed: %s", ph_status_str(err));                     \
            (out) = (err);                                                      \
            goto label;                                                         \
        }                                                                       \
    } while (0)

/*
 * PH_PROPAGATE_GOTO(level, expr, out, label)
 *   Evaluate a PhStatus expression. If it fails, log at level, write the
 *   status into out, and jump to label.
 *
 *   PH_PROPAGATE_GOTO(PH_LOG_ERROR, ph_create_device(&dev), status, cleanup);
 */
#define PH_PROPAGATE_GOTO(level, expr, out, label)                              \
    do {                                                                        \
        PhStatus _s = (expr);                                                   \
        if (_s != PH_SUCCESS) {                                                 \
            ph_log(level, __FILE__, __LINE__,                                   \
                   "Status error: %s", ph_status_str(_s));                      \
            (out) = _s;                                                         \
            goto label;                                                         \
        }                                                                       \
    } while (0)

/*
 * PH_CHECK_OR_RETURN(level, cond, err)
 *   Return err if cond is false, logging at level.
 *
 *   PH_CHECK_OR_RETURN(PH_LOG_ERROR, width > 0, PH_ERR_INVALID_ARG);
 */
#define PH_CHECK_OR_RETURN(level, cond, err)                                    \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ph_log(level, __FILE__, __LINE__,                                   \
                   "Check failed (" #cond "): %s", ph_status_str(err));         \
            return (err);                                                       \
        }                                                                       \
    } while (0)

/*
 * PH_NULL_CHECK(level, ptr)
 *   Log at level and return PH_ERR_INVALID_ARG if ptr is NULL.
 */
#define PH_NULL_CHECK(level, ptr)                                               \
    do {                                                                        \
        if (!(ptr)) {                                                           \
            ph_log(level, __FILE__, __LINE__, "Null pointer: " #ptr);           \
            return PH_ERR_INVALID_ARG;                                          \
        }                                                                       \
    } while (0)

/*
 * PH_NULL_CHECK_GOTO(level, ptr, out, label)
 *   Jump to label if ptr is NULL. Logs at level and writes PH_ERR_INVALID_ARG into out.
 */
#define PH_NULL_CHECK_GOTO(level, ptr, out, label)                              \
    do {                                                                        \
        if (!(ptr)) {                                                           \
            ph_log(level, __FILE__, __LINE__, "Null pointer: " #ptr);           \
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
