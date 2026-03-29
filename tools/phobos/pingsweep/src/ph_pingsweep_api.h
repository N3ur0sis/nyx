/**
 * @file ph_pingsweep_api.h
 * @brief Public API for Phobos Ping Sweep tool
 * @author Neur0sis (2025)
 *
 * The Ping Sweep tool performs network discovery on a subnet by sending
 * ICMP Echo Request (ping) packets to a range of IP addresses. It
 * identifies which hosts are alive and reachable, providing a simple
 * way to map out devices on a network.
 *
 * Features:
 * - CIDR-based subnet scanning
 * - Multi-threaded parallel scanning
 * - Configurable timeout and thread count
 * - Per-host latency measurement
 * - Formatted result output
 *
 * On error, this module sets detailed error information in the nyx_error
 * system, retrievable via nyx_error_get() and nyx_error_log().
 *
 * This tool is part of the Phobos module from the Nyx Offensive
 * Security Framework.
 */

#ifndef PH_PINGSWEEP_API_H
#define PH_PINGSWEEP_API_H

#include <stddef.h>
#include <stdint.h>

/**
 * @name Constants
 * Configuration constants for the ping sweep tool
 * @{
 */
#define PH_PINGSWEEP_MAX_IP_LEN      16     /**< Max IPv4 string length */
#define PH_PINGSWEEP_MAX_MAC_LEN     18     /**< Max MAC string length */
#define PH_PINGSWEEP_MAX_HOSTS       65536  /**< Max hosts in a single sweep */
#define PH_PINGSWEEP_DEFAULT_TIMEOUT  1000  /**< Default timeout in ms */
#define PH_PINGSWEEP_DEFAULT_THREADS  4     /**< Default thread count */
/** @} */

/**
 * @name Status Codes
 * Return values for API functions
 * @{
 */
#define PH_PINGSWEEP_SUCCESS              0  /**< Operation completed successfully */
#define PH_PINGSWEEP_ERR_INVALID_IP      -1  /**< Invalid IP address format */
#define PH_PINGSWEEP_ERR_SOCKET          -2  /**< Socket creation failed */
#define PH_PINGSWEEP_ERR_SEND            -3  /**< Failed to send ICMP packet */
#define PH_PINGSWEEP_ERR_RECEIVE         -4  /**< Failed to receive ICMP response */
#define PH_PINGSWEEP_ERR_TIMEOUT         -5  /**< Ping request timed out */
#define PH_PINGSWEEP_ERR_NO_HOSTS        -6  /**< No hosts found in range */
#define PH_PINGSWEEP_ERR_PERMISSION      -7  /**< Insufficient permissions */
#define PH_PINGSWEEP_ERR_MEMORY          -8  /**< Memory allocation failed */
#define PH_PINGSWEEP_ERR_THREAD          -9  /**< Thread creation failed */
#define PH_PINGSWEEP_ERR_INVALID_PARAM  -10  /**< Invalid parameter provided */
#define PH_PINGSWEEP_ERR_CANCELED       -11  /**< Operation was canceled */
#define PH_PINGSWEEP_ERR_CIDR           -12  /**< Invalid CIDR notation */
/** @} */

/**
 * Result for a single host
 */
typedef struct {
    char ip[PH_PINGSWEEP_MAX_IP_LEN]; /**< Host IP address */
    int alive;                          /**< 1 if host responded, 0 otherwise */
    double latency_ms;                  /**< Round-trip time in milliseconds */
} ph_pingsweep_host_t;

/**
 * Aggregated scan results
 */
typedef struct {
    ph_pingsweep_host_t *hosts; /**< Array of per-host results */
    size_t total;                /**< Total hosts scanned */
    size_t alive_count;          /**< Number of hosts that responded */
    double elapsed_ms;           /**< Total scan duration in milliseconds */
} ph_pingsweep_result_t;

/**
 * Scan configuration
 */
typedef struct {
    char cidr[32];               /**< Target in CIDR notation (e.g. 192.168.1.0/24) */
    char iface[16];              /**< Interface (optional, empty for auto) */
    int timeout_ms;              /**< Per-host timeout in milliseconds */
    int threads;                 /**< Number of scanning threads */
} ph_pingsweep_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Performs a ping sweep over the configured CIDR range.
 *
 * Allocates and populates a result structure. The caller must free it
 * with ph_pingsweep_free_result().
 *
 * @param config Scan configuration
 * @param result Pointer to receive allocated results
 * @return PH_PINGSWEEP_SUCCESS on success, or an error code
 */
int ph_pingsweep_scan(const ph_pingsweep_config_t *config,
                      ph_pingsweep_result_t **result);

/**
 * Pings a single host and returns the round-trip time.
 *
 * @param ip         Target IPv4 address string
 * @param timeout_ms Timeout in milliseconds
 * @param latency_ms Pointer to store the latency (set on success)
 * @return PH_PINGSWEEP_SUCCESS if host is alive, or an error/timeout code
 */
int ph_pingsweep_ping_host(const char *ip, int timeout_ms,
                           double *latency_ms);

/**
 * Frees a result structure returned by ph_pingsweep_scan().
 *
 * @param result Result to free (NULL-safe)
 */
void ph_pingsweep_free_result(ph_pingsweep_result_t *result);

/**
 * Prints scan results to stdout in a formatted table.
 *
 * @param result Scan results to display
 */
void ph_pingsweep_print_result(const ph_pingsweep_result_t *result);

/**
 * Lists available network interfaces with details to stdout.
 *
 * @return PH_PINGSWEEP_SUCCESS on success, or an error code
 */
int ph_pingsweep_list_interfaces_stdout(void);

#ifdef __cplusplus
}
#endif

#endif /* PH_PINGSWEEP_API_H */
