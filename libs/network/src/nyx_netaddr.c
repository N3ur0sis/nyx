/**
 * @file nyx_netaddr.c
 * @brief Network address utility implementation
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "nyx_logger.h"
#include "nyx_error.h"
#include "nyx_netaddr.h"

int nyx_netaddr_parse_cidr(const char *cidr, nyx_cidr_info_t *info)
{
    if (!cidr || !info) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_PARAM, "NULL parameter provided");
        return NYX_NETADDR_ERR_PARAM;
    }

    // Initialize the structure
    memset(info, 0, sizeof(*info));

    // Copy the CIDR string for reference
    strncpy(info->cidr_str, cidr, NYX_MAX_CIDR_LEN - 1);
    info->cidr_str[NYX_MAX_CIDR_LEN - 1] = '\0';

    // Find the '/' separator
    char *slash = strchr(info->cidr_str, '/');
    if (!slash) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_INVALID_STATE,
                      "Invalid CIDR format, missing prefix length: %s", cidr);
        return NYX_NETADDR_ERR_FORMAT;
    }

    // Split the string at the slash
    *slash = '\0';

    // Parse the network address part
    struct in_addr addr;
    if (inet_pton(AF_INET, info->cidr_str, &addr) != 1) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_INVALID_STATE, "Invalid IP address in CIDR: %s",
                      info->cidr_str);
        return NYX_NETADDR_ERR_FORMAT;
    }
    info->network = ntohl(addr.s_addr);

    char *endptr;
    long prefix_long = strtol(slash + 1, &endptr, 10);
    if (*endptr != '\0' || endptr == slash + 1 || prefix_long < 0 || prefix_long > 32) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_PARAM, "Invalid prefix length in CIDR: %s",
                      slash + 1);
        return NYX_NETADDR_ERR_RANGE;
    }
    info->prefix_len = (uint8_t)prefix_long;

    // Restore the '/' in the CIDR string
    *slash = '/';

    // Calculate the subnet mask
    if (info->prefix_len == 0) {
        info->netmask = 0;
    } else {
        info->netmask = (uint32_t)(~((1UL << (32 - info->prefix_len)) - 1));
    }

    // Ensure the network address is properly masked
    info->network &= info->netmask;

    // Calculate derived values
    info->broadcast = info->network | ~info->netmask;

    if (info->prefix_len < 31) {
        // Normal network with usable host range and broadcast
        info->first_host = info->network + 1;
        info->last_host = info->broadcast - 1;
        info->num_hosts = info->last_host - info->first_host + 1;
    } else if (info->prefix_len == 31) {
        // Point-to-point network with no broadcast
        info->first_host = info->network;
        info->last_host = info->network + 1;
        info->num_hosts = 2;
    } else if (info->prefix_len == 32) {
        // Host-specific route (single IP)
        info->first_host = info->network;
        info->last_host = info->network;
        info->num_hosts = 1;
    }

    return NYX_NETADDR_SUCCESS;
}

int nyx_netaddr_prefix_to_mask(uint8_t prefix_len, uint32_t *mask)
{
    if (!mask) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, "NULL mask pointer provided");
        return NYX_NETADDR_ERR_PARAM;
    }

    if (prefix_len > 32) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, "Invalid prefix length: %d", prefix_len);
        return NYX_NETADDR_ERR_RANGE;
    }

    if (prefix_len == 0) {
        *mask = 0;
    } else {
        *mask = (uint32_t)(~((1UL << (32 - prefix_len)) - 1));
    }

    return NYX_NETADDR_SUCCESS;
}

int nyx_netaddr_str_to_ip(const char *ip_str, uint32_t *ip)
{
    if (!ip_str || !ip) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, "NULL parameter provided");
        return NYX_NETADDR_ERR_PARAM;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, "Invalid IP address format: %s", ip_str);
        return NYX_NETADDR_ERR_FORMAT;
    }

    *ip = ntohl(addr.s_addr);
    return NYX_NETADDR_SUCCESS;
}

int nyx_netaddr_ip_to_str(uint32_t ip, char *ip_str, size_t len)
{
    if (!ip_str || len < NYX_MAX_IP_LEN) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, "Invalid buffer or buffer size");
        return NYX_NETADDR_ERR_PARAM;
    }

    struct in_addr addr;
    addr.s_addr = htonl(ip);

    if (!inet_ntop(AF_INET, &addr, ip_str, (socklen_t)len)) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, "Failed to convert IP address to string");
        return NYX_NETADDR_ERR_FORMAT;
    }

    return NYX_NETADDR_SUCCESS;
}

int nyx_netaddr_next_ip(uint32_t *current, uint32_t last_ip)
{
    if (!current) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, "NULL current IP pointer provided");
        return 0;
    }

    if (*current >= last_ip) {
        return 0; // End of range reached
    }

    (*current)++;
    return 1;
}

int nyx_netaddr_validate_ipv4(const char *ip_str)
{
    struct in_addr addr;
    return ip_str && inet_pton(AF_INET, ip_str, &addr) == 1;
}

int nyx_netaddr_validate_mac(const char *mac_str)
{
    if (!mac_str) {
        return 0;
    }

    // Check length
    size_t len = strlen(mac_str);
    if (len != 17) { // XX:XX:XX:XX:XX:XX = 17 chars
        return 0;
    }

    // Check format (XX:XX:XX:XX:XX:XX)
    for (int i = 0; i < 17; i++) {
        if ((i + 1) % 3 == 0) {
            // Positions 2, 5, 8, 11, 14 should be colons
            if (mac_str[i] != ':') {
                return 0;
            }
        } else {
            // All other positions should be hex digits
            if (!isxdigit((unsigned char)mac_str[i])) {
                return 0;
            }
        }
    }

    return 1;
}

int nyx_netaddr_validate_cidr(const char *cidr)
{
    if (!cidr) {
        return 0;
    }

    // Find the prefix separator
    const char *slash = strchr(cidr, '/');
    if (!slash) {
        return 0;
    }

    // Validate IP part
    char ip_part[NYX_MAX_IP_LEN];
    size_t ip_len = (size_t)(slash - cidr);

    if (ip_len >= NYX_MAX_IP_LEN) {
        return 0;
    }

    memcpy(ip_part, cidr, ip_len);
    ip_part[ip_len] = '\0';

    if (!nyx_netaddr_validate_ipv4(ip_part)) {
        return 0;
    }

    // Validate prefix part
    const char *prefix_str = slash + 1;
    char *endptr;
    long prefix = strtol(prefix_str, &endptr, 10);

    if (*endptr != '\0' || prefix < 0 || prefix > 32) {
        return 0;
    }

    return 1;
}
