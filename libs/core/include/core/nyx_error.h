/**
 * @file nyx_error.h
 * @brief Error handling and management for Nyx framework
 * @author Neur0sis (2025)
 *
 * This module provides unified error handling capabilities across the Nyx framework,
 * allowing standard error codes, translations between module domains, and error
 * context management. It integrates with nyx_logger for reporting errors.
 */

#ifndef NYX_ERROR_H
#define NYX_ERROR_H

#include <stddef.h>

/**
 * @name Core Error Codes
 * Standard error codes used throughout the Nyx framework
 * @{
 */
#define NYX_SUCCESS           0   /**< Operation successful */
#define NYX_ERR_GENERIC       -1  /**< General/unspecified error */
#define NYX_ERR_PARAM         -2  /**< Invalid parameter */
#define NYX_ERR_MEMORY        -3  /**< Memory allocation failure */
#define NYX_ERR_IO            -4  /**< I/O error */
#define NYX_ERR_PERMISSION    -5  /**< Permission denied */
#define NYX_ERR_NOT_FOUND     -6  /**< Resource not found */
#define NYX_ERR_NOT_SUPPORTED -7  /**< Operation not supported */
#define NYX_ERR_TIMEOUT       -8  /**< Operation timed out */
#define NYX_ERR_BUSY          -9  /**< Resource busy or locked */
#define NYX_ERR_NETWORK       -10 /**< Network error */
#define NYX_ERR_PROTOCOL      -11 /**< Protocol error */
#define NYX_ERR_INVALID_STATE -12 /**< Invalid state for operation */
#define NYX_ERR_CANCELED      -13 /**< Operation canceled */
/** @} */

/**
 * @name Error Domain Identifiers
 * Domain identifiers for various Nyx modules
 * @{
 */
#define NYX_DOMAIN_CORE      0 /**< Core framework errors */
#define NYX_DOMAIN_IFACE     1 /**< Network interface errors */
#define NYX_DOMAIN_MACSPOOF  2 /**< MAC spoofing errors */
#define NYX_DOMAIN_ARPSPOOF  3 /**< ARP spoofing errors */
#define NYX_DOMAIN_PINGSWEEP 4 /**< Ping sweep errors */
#define NYX_DOMAIN_PORTSCAN  5 /**< Port scanning errors */
#define NYX_DOMAIN_NETADDR   6 /**< Network address errors */
/** @} */

/**
 * Error severity levels
 */
typedef enum {
    NYX_ERROR_SEV_INFO,    /**< Informational message */
    NYX_ERROR_SEV_WARNING, /**< Warning - operation may continue but with issues */
    NYX_ERROR_SEV_ERROR,   /**< Error - operation failed but system can continue */
    NYX_ERROR_SEV_CRITICAL /**< Critical error - system stability may be affected */
} nyx_error_severity_t;

/**
 * Error context structure storing information about the most recent error
 */
typedef struct {
    int code;                      /**< Error code */
    int domain;                    /**< Error domain */
    nyx_error_severity_t severity; /**< Error severity */
    const char *func;              /**< Function where error occurred */
    const char *file;              /**< Source file */
    int line;                      /**< Line number */
    char message[256];             /**< Detailed error message */
    char suggestion[256];          /**< Suggested fix or workaround */
} nyx_error_context_t;

/**
 * Error code map item for domain translations
 */
typedef struct {
    int src_code; /**< Source error code */
    int dst_code; /**< Destination error code */
} nyx_error_map_item_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sets the current error context with provided details
 *
 * @param domain Error domain identifier
 * @param code Error code
 * @param file Source file where error occurred
 * @param line Line number where error occurred
 * @param func Function name where error occurred
 * @param fmt Format string for error message (printf style)
 * @param ... Format arguments
 * @return The error code passed in
 */
int nyx_error_set(int domain, int code, const char *file, int line, const char *func,
                  const char *fmt, ...);

/**
 * Sets the current error context with severity and suggestion
 *
 * @param domain Error domain identifier
 * @param code Error code
 * @param severity Error severity level
 * @param file Source file where error occurred
 * @param line Line number where error occurred
 * @param func Function name where error occurred
 * @param message Error message
 * @param suggestion Suggested fix or workaround
 * @return The error code passed in
 */
int nyx_error_set_ex(int domain, int code, nyx_error_severity_t severity, const char *file,
                     int line, const char *func, const char *message, const char *suggestion);

/**
 * Gets the current error context
 *
 * @return Pointer to the current error context structure
 */
const nyx_error_context_t *nyx_error_get(void);

/**
 * Clears the current error context
 */
void nyx_error_clear(void);

/**
 * Translates an error code from one domain to the core domain
 *
 * @param domain Source error domain
 * @param code Error code in the source domain
 * @return Equivalent error code in the core domain
 */
int nyx_error_to_core(int domain, int code);

/**
 * Translates an error code from one domain to another using a mapping table
 *
 * @param code Error code to translate
 * @param map Array of mapping items
 * @param map_size Number of items in the mapping array
 * @param default_code Default code to return if no mapping is found
 * @return Translated error code
 */
int nyx_error_translate(int code, const nyx_error_map_item_t *map, size_t map_size,
                        int default_code);

/**
 * Returns a human-readable string for a given error code
 *
 * @param domain Error domain
 * @param code Error code
 * @return Constant string describing the error
 */
const char *nyx_error_str(int domain, int code);

/**
 * Sets a suggested fix for the current error
 *
 * @param fmt Format string for suggestion (printf style)
 * @param ... Format arguments
 */
void nyx_error_suggest(const char *fmt, ...);

/**
 * Sets the severity level for the current error
 *
 * @param severity New severity level
 */
void nyx_error_set_severity(nyx_error_severity_t severity);

/**
 * Logs current error context using nyx_logger
 *
 * @param level Log level to use
 * @param show_details Whether to show technical details (file/line)
 */
void nyx_error_log(int level, int show_details);

/**
 * Error capture macro - captures error with source context
 */
#define NYX_ERROR_SET(domain, code, ...) \
    nyx_error_set(domain, code, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * Extended error capture macro with severity and suggestion
 */
#define NYX_ERROR_SET_EX(domain, code, severity, message, suggestion) \
    nyx_error_set_ex(domain, code, severity, __FILE__, __LINE__, __func__, message, suggestion)

#ifdef __cplusplus
}
#endif

#endif /* NYX_ERROR_H */