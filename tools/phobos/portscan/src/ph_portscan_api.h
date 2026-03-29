/**
 * @file ph_portscan_api.h
 * @brief Public API for Phobos Port Scan tool
 * @author Neur0sis (2025)
 *
 * The Port Scan tool determines the state of TCP ports on a single
 * target host: open, closed, or filtered.  It supports two scan modes:
 *
 *   - TCP Connect: uses connect() -- works without root, slower
 *   - TCP SYN (half-open): sends raw SYN packets -- needs root, fast
 *
 * This is an atomic, single-purpose tool.  It does NOT identify
 * services, grab banners, or fingerprint versions -- those are
 * separate tools that chain via the NYX workflow engine.
 *
 * On error, detailed information is set in the nyx_error system.
 *
 * This tool is part of the Phobos module from the Nyx Offensive
 * Security Framework.
 */

#ifndef PH_PORTSCAN_API_H
#define PH_PORTSCAN_API_H

#include <stddef.h>
#include <stdint.h>

/**
 * @name Constants
 * @{
 */
#define PH_PORTSCAN_MAX_PORTS        65535
#define PH_PORTSCAN_DEFAULT_TIMEOUT  2000   /**< Per-port timeout in ms */
#define PH_PORTSCAN_DEFAULT_THREADS  16
#define PH_PORTSCAN_DEFAULT_TOP_PORTS 100
#define PH_PORTSCAN_MAX_IP_LEN       16     /**< Max IPv4 string length */
/** @} */

/**
 * @name Status Codes
 * @{
 */
#define PH_PORTSCAN_SUCCESS             0
#define PH_PORTSCAN_ERR_INVALID_TARGET -1
#define PH_PORTSCAN_ERR_SOCKET         -2
#define PH_PORTSCAN_ERR_PERMISSION     -3
#define PH_PORTSCAN_ERR_TIMEOUT        -4
#define PH_PORTSCAN_ERR_MEMORY         -5
#define PH_PORTSCAN_ERR_THREAD         -6
#define PH_PORTSCAN_ERR_INVALID_PARAM  -7
#define PH_PORTSCAN_ERR_CANCELED       -8
/** @} */

/**
 * Scan mode
 */
typedef enum {
    PH_PORTSCAN_TCP_CONNECT = 0,  /**< connect() -- no root needed */
    PH_PORTSCAN_TCP_SYN     = 1   /**< raw SYN -- needs root, fast+stealth */
} ph_portscan_mode_t;

/**
 * Port state as determined by the scan
 */
typedef enum {
    PH_PORT_OPEN     = 0,
    PH_PORT_CLOSED   = 1,
    PH_PORT_FILTERED = 2
} ph_port_state_t;

/**
 * Result for a single port
 */
typedef struct {
    uint16_t        port;
    ph_port_state_t state;
} ph_portscan_port_t;

/**
 * Aggregated scan results
 */
typedef struct {
    char target[PH_PORTSCAN_MAX_IP_LEN];
    ph_portscan_port_t *ports;  /**< Array of per-port results */
    size_t scanned_count;       /**< Total ports scanned */
    size_t open_count;          /**< Ports in OPEN state */
    ph_portscan_mode_t actual_mode; /**< Effective mode after fallbacks */
    double elapsed_ms;          /**< Total scan duration */
} ph_portscan_result_t;

/**
 * Scan configuration
 */
typedef struct {
    char target[PH_PORTSCAN_MAX_IP_LEN];
    uint16_t port_start;        /**< Range start (0 if using top_ports) */
    uint16_t port_end;          /**< Range end */
    int top_ports;              /**< If > 0, scan top N common ports instead of range */
    ph_portscan_mode_t mode;
    int timeout_ms;
    int threads;
} ph_portscan_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Performs a port scan against a single target host.
 *
 * Allocates and populates a result structure.  The caller must
 * free it with ph_portscan_free_result().
 *
 * @param config Scan configuration
 * @param result Pointer to receive allocated results
 * @return PH_PORTSCAN_SUCCESS on success, or a negative error code
 */
int ph_portscan_scan(const ph_portscan_config_t *config,
                     ph_portscan_result_t **result);

/**
 * Frees a result structure returned by ph_portscan_scan().
 *
 * @param result Result to free (NULL-safe)
 */
void ph_portscan_free_result(ph_portscan_result_t *result);

/**
 * Prints scan results to stdout in a formatted table.
 *
 * @param result  Scan results to display
 * @param open_only If non-zero, only print ports with OPEN state
 */
void ph_portscan_print_result(const ph_portscan_result_t *result,
                              int open_only);

#ifdef __cplusplus
}
#endif

#endif /* PH_PORTSCAN_API_H */
