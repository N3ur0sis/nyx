/**
 * @file nyx_netaddr.h
 * @brief Network address utility for Nyx framework
 * @author Neur0sis (2025)
 *
 * Provides CIDR notation parsing, subnet calculations, and address
 * conversions used across all network-aware tools.
 */

#ifndef NYX_NETADDR_H
#define NYX_NETADDR_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

/**
 * @name Status Codes
 * @{
 */
#define NYX_NETADDR_SUCCESS       0
#define NYX_NETADDR_ERR_PARAM    -1
#define NYX_NETADDR_ERR_FORMAT   -2
#define NYX_NETADDR_ERR_RANGE    -3
/** @} */

#define NYX_MAX_IP_LEN   16
#define NYX_MAX_MAC_LEN  18
#define NYX_MAX_CIDR_LEN (NYX_MAX_IP_LEN + 4)

typedef struct {
    char cidr_str[NYX_MAX_CIDR_LEN];
    uint32_t network;
    uint32_t netmask;
    uint32_t first_host;
    uint32_t last_host;
    uint32_t broadcast;
    uint8_t prefix_len;
    uint32_t num_hosts;
} nyx_cidr_info_t;

#ifdef __cplusplus
extern "C" {
#endif

int nyx_netaddr_parse_cidr(const char *cidr, nyx_cidr_info_t *info);
int nyx_netaddr_prefix_to_mask(uint8_t prefix_len, uint32_t *mask);
int nyx_netaddr_str_to_ip(const char *ip_str, uint32_t *ip);
int nyx_netaddr_ip_to_str(uint32_t ip, char *ip_str, size_t len);
int nyx_netaddr_next_ip(uint32_t *current, uint32_t last_ip);
int nyx_netaddr_validate_ipv4(const char *ip_str);
int nyx_netaddr_validate_mac(const char *mac_str);
int nyx_netaddr_validate_cidr(const char *cidr);

#ifdef __cplusplus
}
#endif

#endif /* NYX_NETADDR_H */
