/**
 * @file nyx_logger.c
 * @brief Core logging module for Nyx Framework
 * @author Neur0sis (2025)
 */

#include "nyx_logger.h"
#include "nyx_term.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

int nyx_logger_verbose = 0;

void nyx_set_verbose(int v)
{
    nyx_logger_verbose = v;
}

void nyx_log(nyx_log_level_t level, const char *fmt, ...)
{
    if (nyx_logger_verbose < 0)
        return;
    if (level == NYX_LOG_VERBOSE && !nyx_logger_verbose)
        return;

    const char *prefix;
    const char *color;

    switch (level) {
    case NYX_LOG_ERROR:
        prefix = "[-]";
        color = COLOR_RED;
        break;
    case NYX_LOG_WARN:
        prefix = "[!]";
        color = COLOR_YELLOW;
        break;
    case NYX_LOG_INFO:
        prefix = "[+]";
        color = COLOR_RESET;
        break;
    case NYX_LOG_VERBOSE:
        prefix = "[*]";
        color = COLOR_CYAN;
        break;
    case NYX_LOG_SUCCESS:
        prefix = "[✓]";
        color = COLOR_GREEN;
        break;
    default:
        prefix = "[?]";
        color = COLOR_RESET;
        break;
    }

    /* Thread-safe timestamp using localtime_r and stack buffer */
    char ts_buf[32];
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    va_list args;
    va_start(args, fmt);

    nyx_term_suspend();
    flockfile(stdout);
    fprintf(stdout, "%s%s (%s) ", color, prefix, ts_buf);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "%s\n", COLOR_RESET);
    funlockfile(stdout);
    nyx_term_resume();

    va_end(args);
}
