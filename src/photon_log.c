#include <photon/photon_log.h>

#include <stdio.h>
#include <unistd.h>

/* ---- ANSI colour codes --------------------------------------------------- */

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[31m"
#define ANSI_MAGENTA "\033[35m"

/* ---- Level metadata ------------------------------------------------------ */

typedef struct
{
    const char *label;
    const char *color;
} LevelMeta;

static const LevelMeta s_meta[] = {
    [PH_LOG_VERBOSE] = { "VERBOSE", ANSI_DIM                },
    [PH_LOG_DEBUG]   = { "DEBUG",   ANSI_CYAN               },
    [PH_LOG_INFO]    = { "INFO",    ANSI_GREEN               },
    [PH_LOG_WARN]    = { "WARN",    ANSI_YELLOW              },
    [PH_LOG_ERROR]   = { "ERROR",   ANSI_RED                 },
    [PH_LOG_FATAL]   = { "FATAL",   ANSI_BOLD ANSI_MAGENTA   },
};

/* ---- State --------------------------------------------------------------- */

static PhLogLevel s_min_level = PH_LOG_INFO;

void ph_log_set_level(PhLogLevel level)
{
    s_min_level = level;
}

/* ---- Core ---------------------------------------------------------------- */

void ph_log(PhLogLevel level, const char *file, int line, const char *fmt, ...)
{
    if (level < s_min_level)
        return;

    LevelMeta   meta   = s_meta[level];

    /* Prefix: [LEVEL] for lower levels, [LEVEL file:line] for WARN and above */
    fprintf(stdout, "%s[%-7s]%s ", meta.color, meta.label, ANSI_RESET);

    if (level >= PH_LOG_WARN)
    {
        fprintf(stdout, ANSI_DIM "%s:%d " ANSI_RESET, file, line);
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fputc('\n', stdout);
}
