// nyx_logger.h - Unified logger for Nyx framework
// Author: Neur0sis (2025)

#ifndef NYX_LOGGER_H
#define NYX_LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_MAGENTA "\033[35m"

typedef enum {
    NYX_LOG_INFO,
    NYX_LOG_WARN,
    NYX_LOG_ERROR,
    NYX_LOG_SUCCESS,
    NYX_LOG_VERBOSE
} nyx_log_level_t;

extern int nyx_logger_verbose;

void nyx_log(nyx_log_level_t level, const char *fmt, ...);
void nyx_set_verbose(int v);

#endif
