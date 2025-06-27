/**
 * @file nyx_error.c
 * @brief Error handling and management implementation
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

#include "nyx_error.h"
#include "nyx_logger.h"
#include "../utils/ph_iface.h"
#include "../tools/phobos/macspoof/src/ph_macspoof_api.h"

// Thread-local error context
static __thread nyx_error_context_t current_error = {0};

// String representations of common errors
static const char* core_error_strings[] = {
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
    [-NYX_ERR_CANCELED] = "Operation canceled"
};

// Default suggestions for common error types
static const char* default_suggestions[] = {
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
    [-NYX_ERR_CANCELED] = "Operation was canceled by user or system"
};

// Domain to string mappings
static const char* domain_names[] = {
    [NYX_DOMAIN_CORE] = "Core",
    [NYX_DOMAIN_IFACE] = "Interface",
    [NYX_DOMAIN_MACSPOOF] = "MAC Spoofer",
    [NYX_DOMAIN_ARPSPOOF] = "ARP Spoofer",
    [NYX_DOMAIN_PINGSWEEP] = "Ping Sweep",
    [NYX_DOMAIN_PORTSCAN] = "Port Scanner"
};

// Standard error mappings from various domains to core domain
static const nyx_error_map_item_t iface_to_core_map[] = {
    {PH_IFACE_SUCCESS, NYX_SUCCESS},
    {PH_IFACE_ERR_GENERIC, NYX_ERR_GENERIC},
    {PH_IFACE_ERR_PARAM, NYX_ERR_PARAM},
    {PH_IFACE_ERR_NOTFOUND, NYX_ERR_NOT_FOUND},
    {PH_IFACE_ERR_IO, NYX_ERR_IO},
    {PH_IFACE_ERR_PERM, NYX_ERR_PERMISSION},
    {PH_IFACE_ERR_SOCKET, NYX_ERR_NETWORK},
    {PH_IFACE_ERR_BUSY, NYX_ERR_BUSY}
};

static const nyx_error_map_item_t macspoof_to_core_map[] = {
    {PH_SUCCESS, NYX_SUCCESS},
    {PH_ERR_INVALID_MAC, NYX_ERR_PARAM},
    {PH_ERR_NO_IFACE, NYX_ERR_NOT_FOUND},
    {PH_ERR_SOCKET, NYX_ERR_NETWORK},
    {PH_ERR_IOCTL, NYX_ERR_IO},
    {PH_ERR_FILE_IO, NYX_ERR_IO},
    {PH_ERR_PERMISSION, NYX_ERR_PERMISSION},
    {PH_ERR_NOT_FOUND, NYX_ERR_NOT_FOUND},
    {PH_ERR_ALREADY_SAVED, NYX_ERR_GENERIC},
    {PH_ERR_BUSY, NYX_ERR_BUSY}
};

int nyx_error_set(int domain, int code, const char *file, int line,
                 const char *func, const char *fmt, ...) {
    // Clear previous error
    memset(&current_error, 0, sizeof(current_error));
    
    // Set the basic fields
    current_error.code = code;
    current_error.domain = domain;
    current_error.file = file;
    current_error.line = line;
    current_error.func = func;
    current_error.severity = NYX_ERROR_SEV_ERROR; // Default severity
    
    // Format the error message
    va_list args;
    va_start(args, fmt);
    vsnprintf(current_error.message, sizeof(current_error.message), fmt, args);
    va_end(args);
    
    // Set default suggestion based on translated core error
    int core_code = nyx_error_to_core(domain, code);
    if (core_code < 0 && -core_code < (int)(sizeof(default_suggestions)/sizeof(default_suggestions[0]))) {
        const char *suggestion = default_suggestions[-core_code];
        if (suggestion) {
            strncpy(current_error.suggestion, suggestion, sizeof(current_error.suggestion) - 1);
        }
    }
    
    return code;
}

int nyx_error_set_ex(int domain, int code, nyx_error_severity_t severity,
                    const char *file, int line, const char *func,
                    const char *message, const char *suggestion) {
    // Clear previous error
    memset(&current_error, 0, sizeof(current_error));
    
    // Set all fields
    current_error.code = code;
    current_error.domain = domain;
    current_error.severity = severity;
    current_error.file = file;
    current_error.line = line;
    current_error.func = func;
    
    if (message) {
        strncpy(current_error.message, message, sizeof(current_error.message) - 1);
    }
    
    if (suggestion) {
        strncpy(current_error.suggestion, suggestion, sizeof(current_error.suggestion) - 1);
    } else {
        // Set default suggestion
        int core_code = nyx_error_to_core(domain, code);
        if (core_code < 0 && -core_code < (int)(sizeof(default_suggestions)/sizeof(default_suggestions[0]))) {
            const char *default_sugg = default_suggestions[-core_code];
            if (default_sugg) {
                strncpy(current_error.suggestion, default_sugg, sizeof(current_error.suggestion) - 1);
            }
        }
    }
    
    return code;
}

const nyx_error_context_t* nyx_error_get(void) {
    return &current_error;
}

void nyx_error_clear(void) {
    memset(&current_error, 0, sizeof(current_error));
}

void nyx_error_suggest(const char *fmt, ...) {
    if (!fmt) return;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(current_error.suggestion, sizeof(current_error.suggestion), fmt, args);
    va_end(args);
}

void nyx_error_set_severity(nyx_error_severity_t severity) {
    current_error.severity = severity;
}

int nyx_error_to_core(int domain, int code) {
    const nyx_error_map_item_t *map = NULL;
    size_t map_size = 0;
    
    // Already core domain
    if (domain == NYX_DOMAIN_CORE) {
        return code;
    }
    
    // Select the appropriate mapping table
    switch (domain) {
        case NYX_DOMAIN_IFACE:
            map = iface_to_core_map;
            map_size = sizeof(iface_to_core_map) / sizeof(iface_to_core_map[0]);
            break;
        case NYX_DOMAIN_MACSPOOF:
            map = macspoof_to_core_map;
            map_size = sizeof(macspoof_to_core_map) / sizeof(macspoof_to_core_map[0]);
            break;
        default:
            return NYX_ERR_GENERIC;  // Unknown domain, return generic error
    }
    
    return nyx_error_translate(code, map, map_size, NYX_ERR_GENERIC);
}

int nyx_error_translate(int code, const nyx_error_map_item_t *map, 
                        size_t map_size, int default_code) {
    if (!map || map_size == 0) {
        return default_code;
    }
    
    // Look up the code in the mapping table
    for (size_t i = 0; i < map_size; i++) {
        if (map[i].src_code == code) {
            return map[i].dst_code;
        }
    }
    
    // No mapping found
    return default_code;
}

const char* nyx_error_str(int domain, int code) {
    static char unknown[64];
    
    // Core domain has direct string mappings
    if (domain == NYX_DOMAIN_CORE) {
        if (code >= 0 || -code >= (int)(sizeof(core_error_strings)/sizeof(core_error_strings[0]))) {
            snprintf(unknown, sizeof(unknown), "Unknown core error %d", code);
            return unknown;
        }
        
        const char *str = core_error_strings[-code];
        return str ? str : "Unknown error";
    }
    
    // For other domains, translate to core first
    int core_code = nyx_error_to_core(domain, code);
    
    // Get the domain name if available
    const char *domain_name = "Unknown domain";
    if (domain >= 0 && domain < (int)(sizeof(domain_names)/sizeof(domain_names[0]))) {
        domain_name = domain_names[domain] ? domain_names[domain] : "Unknown domain";
    }
    
    // Get core error string
    const char *core_str = nyx_error_str(NYX_DOMAIN_CORE, core_code);
    
    // Format with both original and translated information - avoid truncation
    snprintf(unknown, sizeof(unknown), "%.*s error %d (Core: %.*s)", 
            15, domain_name, code, 30, core_str);
    return unknown;
}

void nyx_error_log(int level, int show_details) {
    const nyx_error_context_t *err = nyx_error_get();
    
    if (err->code == NYX_SUCCESS) {
        return; // No error to log
    }
    
    const char *domain_name = "Unknown domain";
    if (err->domain >= 0 && 
        err->domain < (int)(sizeof(domain_names)/sizeof(domain_names[0])) && 
        domain_names[err->domain]) {
        domain_name = domain_names[err->domain];
    }
    
    // Format a user-friendly error message
    char formatted_error[512];
    int pos = 0;
    
    // Add primary error message
    if (err->message[0]) {
        pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos, 
                      "%s", err->message);
    } else {
        pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos, 
                      "%s", nyx_error_str(err->domain, err->code));
    }
    
    // Add suggestion if available
    if (err->suggestion[0]) {
        pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos, 
                      ". %s", err->suggestion);
    }
    
    // For verbose/debug mode, add technical details
    if (show_details) {
        pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos,
                      " (Error code %d in %s domain", 
                      err->code, domain_name);
                      
        if (err->func && err->file) {
            pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos,
                          ", in %s at %s:%d", 
                          err->func, err->file, err->line);
        }
        
        pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos, ")");
    } else {
        // For regular mode, just add the error code 
        pos += snprintf(formatted_error + pos, sizeof(formatted_error) - pos,
                      " (Error code %d)", err->code);
    }
    
    // Log with appropriate level
    nyx_log(level, "%s", formatted_error);
}