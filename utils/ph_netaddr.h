/**
 * @file ph_netaddr.h
 * @brief Network address utility for Nyx framework
 * @author Neur0sis (2025)
 *
 * This module provides functionality for handling network addresses,
 * including CIDR notation parsing, subnet calculations, and address
 * conversions.
 */

#ifndef PH_NETADDR_H
#define PH_NETADDR_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

/**
 * @name Network Address Status Codes
 * Return values for network address functions
 * @{
 */
#define PH_NETADDR_SUCCESS       0  /**< Operation completed successfully */
#define PH_NETADDR_ERR_PARAM    -1  /**< Invalid parameter */
#define PH_NETADDR_ERR_FORMAT   -2  /**< Invalid address format */
#define PH_NETADDR_ERR_RANGE    -3  /**< Invalid address range */
/** @} */

/**
 * Maximum length of IPv4 address string (xxx.xxx.xxx.xxx\0)
 */
#define PH_MAX_IP_LEN 16

/**
 * Maximum length of MAC address string (XX:XX:XX:XX:XX:XX\0)
 */
#define PH_MAX_MAC_LEN 18

/**
 * Maximum length of CIDR notation string (xxx.xxx.xxx.xxx/xx\0)
 */
#define PH_MAX_CIDR_LEN (PH_MAX_IP_LEN + 4)

/**
 * CIDR information structure
 */
typedef struct {
    char cidr_str[PH_MAX_CIDR_LEN];  /**< Original CIDR string */
    uint32_t network;                 /**< Network address (host byte order) */
    uint32_t netmask;                 /**< Subnet mask (host byte order) */
    uint32_t first_host;              /**< First usable host address */
    uint32_t last_host;               /**< Last usable host address */
    uint32_t broadcast;               /**< Broadcast address */
    uint8_t prefix_len;               /**< CIDR prefix length */
    uint32_t num_hosts;               /**< Number of usable hosts */
} ph_cidr_info_t;

/**
 * Parses a CIDR notation string into network and prefix length
 *
 * @param cidr CIDR notation string (e.g., "192.168.1.0/24")
 * @param info Pointer to store CIDR information
 * @return PH_NETADDR_SUCCESS on success, or error code on failure
 */
int ph_netaddr_parse_cidr(const char *cidr, ph_cidr_info_t *info);

/**
 * Converts a CIDR prefix length to a subnet mask
 *
 * @param prefix_len CIDR prefix length (0-32)
 * @param mask Pointer to store the subnet mask (host byte order)
 * @return PH_NETADDR_SUCCESS on success, or error code on failure
 */
int ph_netaddr_prefix_to_mask(uint8_t prefix_len, uint32_t *mask);

/**
 * Converts an IPv4 address string to a 32-bit integer
 *
 * @param ip_str IPv4 address string (e.g., "192.168.1.1")
 * @param ip Pointer to store the IP address (host byte order)
 * @return PH_NETADDR_SUCCESS on success, or error code on failure
 */
int ph_netaddr_str_to_ip(const char *ip_str, uint32_t *ip);

/**
 * Converts a 32-bit integer to an IPv4 address string
 *
 * @param ip IP address (host byte order)
 * @param ip_str Buffer to store the IP address string
 * @param len Length of the buffer (should be >= PH_MAX_IP_LEN)
 * @return PH_NETADDR_SUCCESS on success, or error code on failure
 */
int ph_netaddr_ip_to_str(uint32_t ip, char *ip_str, size_t len);

/**
 * Gets the next IP address in a range
 *
 * @param current Current IP address (host byte order, will be updated)
 * @param last_ip Last IP address in the range (host byte order)
 * @return 1 if next IP is valid, 0 if end of range reached
 */
int ph_netaddr_next_ip(uint32_t *current, uint32_t last_ip);

/**
 * Validates an IPv4 address string format
 *
 * @param ip_str IPv4 address string to validate
 * @return 1 if valid, 0 if invalid
 */
int ph_netaddr_validate_ipv4(const char *ip_str);

/**
 * Validates a MAC address string format
 *
 * @param mac_str MAC address string to validate (XX:XX:XX:XX:XX:XX format)
 * @return 1 if valid, 0 if invalid
 */
int ph_netaddr_validate_mac(const char *mac_str);

/**
 * Validates a CIDR notation string format
 *
 * @param cidr CIDR notation string to validate (e.g., "192.168.1.0/24")
 * @return 1 if valid, 0 if invalid
 */
int ph_netaddr_validate_cidr(const char *cidr);

#endif // PH_NETADDR_H