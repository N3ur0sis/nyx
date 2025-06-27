/**
 * ph_iface.h - Network Interface Utility Library
 * Author: Neur0sis (2025)
 * Version: 1.0
 *
 * Common interface manipulation functions for the NYX framework
 * Provides reusable network utilities for tools like MAC spoofer, ARP spoofer, etc.
 */

#ifndef PH_IFACE_H
#define PH_IFACE_H

#include <stddef.h>

/* Define IFNAMSIZ if it's not already defined */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

// Return codes
#define PH_IFACE_SUCCESS      0
#define PH_IFACE_ERR_GENERIC -1
#define PH_IFACE_ERR_PARAM   -2
#define PH_IFACE_ERR_NOTFOUND -3
#define PH_IFACE_ERR_IO      -4
#define PH_IFACE_ERR_PERM    -5
#define PH_IFACE_ERR_SOCKET  -6
#define PH_IFACE_ERR_BUSY    -7

// Constants
#define PH_IFACE_MAX_MAC_LEN 18

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validates interface name against system constraints and existence
 * 
 * @param iface Interface name to validate
 * @return 1 if valid and exists, 0 otherwise
 */
int ph_iface_is_valid(const char *iface);

/**
 * Validates MAC address format with strict RFC compliance
 * 
 * @param mac MAC address string to validate (XX:XX:XX:XX:XX:XX format)
 * @return 1 if valid, 0 if invalid
 */
int ph_iface_is_valid_mac(const char *mac);

/**
 * Checks if network interface is currently up
 * 
 * @param iface Interface name
 * @return 1 if up, 0 if down or error
 */
int ph_iface_is_up(const char *iface);

/**
 * Gets the current MAC address of an interface
 * 
 * @param iface Interface name
 * @param buffer Buffer to store MAC address
 * @param len Buffer length
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_get_mac(const char *iface, char *buffer, size_t len);

/**
 * Gets the IPv4 address of an interface
 *
 * @param iface Interface name
 * @param buffer Buffer to store IP address (string format)
 * @param len Buffer length
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_get_ipv4(const char *iface, char *buffer, size_t len);

/**
 * Gets the subnet mask of an interface
 *
 * @param iface Interface name
 * @param buffer Buffer to store subnet mask (string format)
 * @param len Buffer length
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_get_netmask(const char *iface, char *buffer, size_t len);

/**
 * Lists all available network interfaces
 *
 * @param interfaces Array to store interface names
 * @param max_count Maximum number of interfaces to return
 * @param count Pointer to store actual count
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_list(char interfaces[][IFNAMSIZ], size_t max_count, size_t *count);

/**
 * Gets MAC address for a given IP on the local network (ARP lookup)
 *
 * @param iface Interface to use for lookup
 * @param ip_addr IP address to look up
 * @param mac_buffer Buffer to store MAC address
 * @param len Buffer length
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_get_mac_by_ip(const char *iface, const char *ip_addr, char *mac_buffer, size_t len);

/**
 * Gets interface index for socket operations
 *
 * @param iface Interface name
 * @return Interface index or 0 on error
 */
unsigned int ph_iface_get_index(const char *iface);

/**
 * Lists interfaces with details (name, MAC, IP, status)
 * Prints directly to stdout - useful for command line tools
 *
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_print_details(void);

/**
 * Gets the default gateway interface
 *
 * @param buffer Buffer to store interface name
 * @param len Buffer length
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_get_default_gateway(char *buffer, size_t len);

/**
 * Gets the default gateway IP address
 *
 * @param buffer Buffer to store gateway IP
 * @param len Buffer length
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_get_gateway_ip(char *buffer, size_t len);

/**
 * Sets interface up or down
 *
 * @param iface Interface name
 * @param up 1 for up, 0 for down
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_set_status(const char *iface, int up);

/**
 * Adds a temporary IPv4 address to an interface
 *
 * @param iface Interface name
 * @param ip_addr IP address to add
 * @param prefix_len Network prefix length (e.g., 24 for /24)
 * @return PH_IFACE_SUCCESS or error code
 */
int ph_iface_add_ipv4(const char *iface, const char *ip_addr, int prefix_len);

#ifdef __cplusplus
}
#endif

#endif /* PH_IFACE_H */