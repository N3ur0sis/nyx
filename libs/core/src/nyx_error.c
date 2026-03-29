/**
 * @file nyx_error.c
 * @brief Error handling and management implementation
 * @author Neur0sis (2025)
 *
 * Decoupled from tool-specific headers. Each tool maps its own domain
 * errors internally; the core error system only knows about
 * NYX_DOMAIN_CORE and provides generic fallback for unknown domains.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

#include "nyx_error.h"
#include "nyx_logger.h"

static __thread nyx_error_context_t current_error = {0};

static const char *core_error_strings[] = {
    [-NYX_SUCCESS] = "Success",
    [-NYX_ERR_GENERIC] = "General error",
    [-NYX_ERR_PARAM] = "Invalid parameter",
    [-NYX_ERR_MEMORY] = "Memory allocation failure",
    [-NYX_ERR_IO] = "I/O error",
    [-NYX_ERR_PERMISSION] = "Permission denied",
    [-NYX_ERR_NOT_FOUND] = "Resource not found",
    [-NYX_ERR_NOT_SUPPORTED] = "Operation not supported",
    [-NYX_ERR_TIMEOUT] = "Operation timed out",
    [-NYX_ERR_BUSY] = "Resource busy or locked",
    [-NYX_ERR_NETWORK] = "Network error",
    [-NYX_ERR_PROTOCOL] = "Protocol error",
    [-NYX_ERR_INVALID_STATE] = "Invalid state for operation",
    [-NYX_ERR_CANCELED] = "Operation canceled",
};

static const char *default_suggestions[] = {
    [-NYX_SUCCESS] = NULL,
    [-NYX_ERR_GENERIC] = "Check logs for more details",
    [-NYX_ERR_PARAM] = "Verify the provided parameters",
    [-NYX_ERR_MEMORY] = "Free memory or reduce memory usage",
    [-NYX_ERR_IO] = "Check file permissions and path",
    [-NYX_ERR_PERMISSION] = "Try running with elevated privileges (sudo)",
    [-NYX_ERR_NOT_FOUND] = "Verify the resource exists and is accessible",
    [-NYX_ERR_NOT_SUPPORTED] = "This function is not available on your system",
    [-NYX_ERR_TIMEOUT] = "Check network connectivity or system load",
    [-NYX_ERR_BUSY] = "Wait and try again later",
    [-NYX_ERR_NETWORK] = "Check network connectivity and settings",
    [-NYX_ERR_PROTOCOL] = "Protocol mismatch or compatibility issue",
    [-NYX_ERR_INVALID_STATE] = "Ensure prerequisites are met before operation",
    [-NYX_ERR_CANCELED] = "Operation was canceled by user or system",
};

static const char *domain_names[] = {
    [NYX_DOMAIN_CORE] = "Core",
    [NYX_DOMAIN_IFACE] = "Interface",
    [NYX_DOMAIN_MACSPOOF] = "MAC Spoofer",
    [NYX_DOMAIN_ARPSPOOF] = "ARP Spoofer",
    [NYX_DOMAIN_PINGSWEEP] = "Ping Sweep",
    [NYX_DOMAIN_PORTSCAN] = "Port Scanner",
    [NYX_DOMAIN_NETADDR] = "Network Address",
};

int nyx_error_set(int domain, int code, const char *file, int line, const char *func,
                  const char *fmt, ...)
{
    memset(&current_error, 0, sizeof(current_error));

    current_error.code = code;
    current_error.domain = domain;
    current_error.file = file;
    current_error.line = line;
    current_error.func = func;
    current_error.severity = NYX_ERROR_SEV_ERROR;

    va_list args;
    va_start(args, fmt);
    vsnprintf(current_error.message, sizeof(current_error.message), fmt, args);
    va_end(args);

    int core_code = nyx_error_to_core(domain, code);
    if (core_code < 0 &&
        -core_code < (int)(sizeof(default_suggestions) / sizeof(default_suggestions[0]))) {
        const char *suggestion = default_suggestions[-core_code];
        if (suggestion)
            strncpy(current_error.suggestion, suggestion, sizeof(current_error.suggestion) - 1);
    }

    return code;
}

int nyx_error_set_ex(int domain, int code, nyx_error_severity_t severity, const char *file,
                     int line, const char *func, const char *message, const char *suggestion)
{
    memset(&current_error, 0, sizeof(current_error));

    current_error.code = code;
    current_error.domain = domain;
    current_error.severity = severity;
    current_error.file = file;
    current_error.line = line;
    current_error.func = func;

    if (message)
        strncpy(current_error.message, message, sizeof(current_error.message) - 1);

    if (suggestion) {
        strncpy(current_error.suggestion, suggestion, sizeof(current_error.suggestion) - 1);
    } else {
        int core_code = nyx_error_to_core(domain, code);
        if (core_code < 0 &&
            -core_code < (int)(sizeof(default_suggestions) / sizeof(default_suggestions[0]))) {
            const char *s = default_suggestions[-core_code];
            if (s)
                strncpy(current_error.suggestion, s, sizeof(current_error.suggestion) - 1);
        }
    }

    return code;
}

const nyx_error_context_t *nyx_error_get(void)
{
    return &current_error;
}

void nyx_error_clear(void)
{
    memset(&current_error, 0, sizeof(current_error));
}

void nyx_error_suggest(const char *fmt, ...)
{
    if (!fmt)
        return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(current_error.suggestion, sizeof(current_error.suggestion), fmt, args);
    va_end(args);
}

void nyx_error_set_severity(nyx_error_severity_t severity)
{
    current_error.severity = severity;
}

int nyx_error_to_core(int domain, int code)
{
    if (domain == NYX_DOMAIN_CORE)
        return code;

    /*
     * Each tool/library maps its own domain codes to core codes
     * internally (e.g. macspoof's map_iface_error_to_macspoof).
     * The core error system returns NYX_ERR_GENERIC for any
     * domain it doesn't know about.
     */
    return NYX_ERR_GENERIC;
}

int nyx_error_translate(int code, const nyx_error_map_item_t *map, size_t map_size,
                        int default_code)
{
    if (!map || map_size == 0)
        return default_code;

    for (size_t i = 0; i < map_size; i++) {
        if (map[i].src_code == code)
            return map[i].dst_code;
    }

    return default_code;
}

const char *nyx_error_str(int domain, int code)
{
    static __thread char unknown[64];

    if (domain == NYX_DOMAIN_CORE) {
        if (code >= 0 ||
            -code >= (int)(sizeof(core_error_strings) / sizeof(core_error_strings[0]))) {
            snprintf(unknown, sizeof(unknown), "Unknown core error %d", code);
            return unknown;
        }
        const char *str = core_error_strings[-code];
        return str ? str : "Unknown error";
    }

    int core_code = nyx_error_to_core(domain, code);

    const char *domain_name = "Unknown domain";
    if (domain >= 0 && domain < (int)(sizeof(domain_names) / sizeof(domain_names[0])))
        domain_name = domain_names[domain] ? domain_names[domain] : "Unknown domain";

    const char *core_str = nyx_error_str(NYX_DOMAIN_CORE, core_code);

    snprintf(unknown, sizeof(unknown), "%.*s error %d (Core: %.*s)", 15, domain_name, code, 30,
             core_str);
    return unknown;
}

void nyx_error_log(int level, int show_details)
{
    const nyx_error_context_t *err = nyx_error_get();

    if (err->code == NYX_SUCCESS)
        return;

    const char *domain_name = "Unknown domain";
    if (err->domain >= 0 && err->domain < (int)(sizeof(domain_names) / sizeof(domain_names[0])) &&
        domain_names[err->domain])
        domain_name = domain_names[err->domain];

    char formatted_error[512];
    size_t pos = 0;
    size_t remaining = sizeof(formatted_error);

#define SAFE_APPEND(...)                                                  \
    do {                                                                  \
        int _n = snprintf(formatted_error + pos, remaining, __VA_ARGS__); \
        if (_n > 0 && (size_t)_n < remaining) {                           \
            pos += (size_t)_n;                                            \
            remaining -= (size_t)_n;                                      \
        } else if (_n > 0) {                                              \
            pos = sizeof(formatted_error) - 1;                            \
            remaining = 0;                                                \
        }                                                                 \
    } while (0)

    if (err->message[0])
        SAFE_APPEND("%s", err->message);
    else
        SAFE_APPEND("%s", nyx_error_str(err->domain, err->code));

    if (err->suggestion[0])
        SAFE_APPEND(". %s", err->suggestion);

    if (show_details) {
        SAFE_APPEND(" (Error code %d in %s domain", err->code, domain_name);
        if (err->func && err->file)
            SAFE_APPEND(", in %s at %s:%d", err->func, err->file, err->line);
        SAFE_APPEND(")");
    } else {
        SAFE_APPEND(" (Error code %d)", err->code);
    }

#undef SAFE_APPEND

    nyx_log(level, "%s", formatted_error);
}
