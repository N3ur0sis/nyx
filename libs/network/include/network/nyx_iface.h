/**
 * @file nyx_iface.h
 * @brief Network Interface Utility Library
 * @author Neur0sis (2025)
 *
 * Common interface manipulation functions for the NYX framework.
 * Provides reusable network utilities for tools across all modules.
 */

#ifndef NYX_IFACE_H
#define NYX_IFACE_H

#include <stddef.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

/**
 * @name Status Codes
 * @{
 */
#define NYX_IFACE_SUCCESS       0
#define NYX_IFACE_ERR_GENERIC  -1
#define NYX_IFACE_ERR_PARAM    -2
#define NYX_IFACE_ERR_NOTFOUND -3
#define NYX_IFACE_ERR_IO       -4
#define NYX_IFACE_ERR_PERM     -5
#define NYX_IFACE_ERR_SOCKET   -6
#define NYX_IFACE_ERR_BUSY     -7
/** @} */

#define NYX_IFACE_MAX_MAC_LEN 18

#ifdef __cplusplus
extern "C" {
#endif

int nyx_iface_is_valid(const char *iface);
int nyx_iface_is_valid_mac(const char *mac);
int nyx_iface_is_up(const char *iface);
int nyx_iface_get_mac(const char *iface, char *buffer, size_t len);
int nyx_iface_get_ipv4(const char *iface, char *buffer, size_t len);
int nyx_iface_get_netmask(const char *iface, char *buffer, size_t len);
int nyx_iface_list(char interfaces[][IFNAMSIZ], size_t max_count, size_t *count);
int nyx_iface_get_mac_by_ip(const char *iface, const char *ip_addr,
                             char *mac_buffer, size_t len);
unsigned int nyx_iface_get_index(const char *iface);
int nyx_iface_print_details(void);
int nyx_iface_get_default_gateway(char *buffer, size_t len);
int nyx_iface_get_gateway_ip(char *buffer, size_t len);
int nyx_iface_set_status(const char *iface, int up);
int nyx_iface_add_ipv4(const char *iface, const char *ip_addr, int prefix_len);

#ifdef __cplusplus
}
#endif

#endif /* NYX_IFACE_H */
