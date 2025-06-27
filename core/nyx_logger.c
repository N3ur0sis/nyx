// nyx_logger.c - Core logging module for Nyx Framework
// Author: Neur0sis (2025)

#include "nyx_logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static const char *get_timestamp() {
    static char buffer[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

void nyx_log(nyx_log_level_t level, const char *fmt, ...) {
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

    va_list args;
    va_start(args, fmt);

    fprintf(stdout, "%s%s (%s) ", color, prefix, get_timestamp());
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "%s\n", COLOR_RESET);

    va_end(args);
}
