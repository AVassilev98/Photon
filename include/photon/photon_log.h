#pragma once

#include <stdarg.h>

typedef enum PhLogLevel
{
    PH_LOG_VERBOSE = 0,
    PH_LOG_DEBUG,
    PH_LOG_INFO,
    PH_LOG_WARN,
    PH_LOG_ERROR,
    PH_LOG_FATAL,
} PhLogLevel;

/* Set the minimum level that gets printed. Default: PH_LOG_INFO. */
void ph_log_set_level(PhLogLevel level);

/* Direct call — prefer the macros below. */
void ph_log(PhLogLevel level, const char *file, int line, const char *fmt, ...);

/* ---- Macros -------------------------------------------------------------- */

#define PH_LOG_VERBOSE(...) ph_log(PH_LOG_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)
#define PH_LOG_DEBUG(...)   ph_log(PH_LOG_DEBUG,   __FILE__, __LINE__, __VA_ARGS__)
#define PH_LOG_INFO(...)    ph_log(PH_LOG_INFO,    __FILE__, __LINE__, __VA_ARGS__)
#define PH_LOG_WARN(...)    ph_log(PH_LOG_WARN,    __FILE__, __LINE__, __VA_ARGS__)
#define PH_LOG_ERROR(...)   ph_log(PH_LOG_ERROR,   __FILE__, __LINE__, __VA_ARGS__)
#define PH_LOG_FATAL(...)   ph_log(PH_LOG_FATAL,   __FILE__, __LINE__, __VA_ARGS__)
