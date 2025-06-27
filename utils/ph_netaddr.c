/**
 * @file ph_netaddr.c
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
#include "ph_netaddr.h"

int ph_netaddr_parse_cidr(const char *cidr, ph_cidr_info_t *info) {
    if (!cidr || !info) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_PARAM, 
                     "NULL parameter provided");
        return PH_NETADDR_ERR_PARAM;
    }
    
    // Initialize the structure
    memset(info, 0, sizeof(*info));
    
    // Copy the CIDR string for reference
    strncpy(info->cidr_str, cidr, PH_MAX_CIDR_LEN - 1);
    info->cidr_str[PH_MAX_CIDR_LEN - 1] = '\0';
    
    // Find the '/' separator
    char *slash = strchr(info->cidr_str, '/');
    if (!slash) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_INVALID_STATE, 
                     "Invalid CIDR format, missing prefix length: %s", cidr);
        return PH_NETADDR_ERR_FORMAT;
    }
    
    // Split the string at the slash
    *slash = '\0';
    
    // Parse the network address part
    struct in_addr addr;
    if (inet_pton(AF_INET, info->cidr_str, &addr) != 1) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_INVALID_STATE, 
                     "Invalid IP address in CIDR: %s", info->cidr_str);
        return PH_NETADDR_ERR_FORMAT;
    }
    info->network = ntohl(addr.s_addr);
    
    // Parse the prefix length
    info->prefix_len = atoi(slash + 1);
    if (info->prefix_len > 32) {
        NYX_ERROR_SET(NYX_DOMAIN_NETADDR, NYX_ERR_PARAM, 
                     "Invalid prefix length in CIDR: %d", info->prefix_len);
        return PH_NETADDR_ERR_RANGE;
    }
    
    // Restore the '/' in the CIDR string
    *slash = '/';
    
    // Calculate the subnet mask
    if (info->prefix_len == 0) {
        info->netmask = 0;
    } else {
        info->netmask = ~((1UL << (32 - info->prefix_len)) - 1);
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
    
    return PH_NETADDR_SUCCESS;
}

int ph_netaddr_prefix_to_mask(uint8_t prefix_len, uint32_t *mask) {
    if (!mask) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "NULL mask pointer provided");
        return PH_NETADDR_ERR_PARAM;
    }
    
    if (prefix_len > 32) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid prefix length: %d", prefix_len);
        return PH_NETADDR_ERR_RANGE;
    }
    
    if (prefix_len == 0) {
        *mask = 0;
    } else {
        *mask = ~((1UL << (32 - prefix_len)) - 1);
    }
    
    return PH_NETADDR_SUCCESS;
}

int ph_netaddr_str_to_ip(const char *ip_str, uint32_t *ip) {
    if (!ip_str || !ip) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "NULL parameter provided");
        return PH_NETADDR_ERR_PARAM;
    }
    
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid IP address format: %s", ip_str);
        return PH_NETADDR_ERR_FORMAT;
    }
    
    *ip = ntohl(addr.s_addr);
    return PH_NETADDR_SUCCESS;
}

int ph_netaddr_ip_to_str(uint32_t ip, char *ip_str, size_t len) {
    if (!ip_str || len < PH_MAX_IP_LEN) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid buffer or buffer size");
        return PH_NETADDR_ERR_PARAM;
    }
    
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    
    if (!inet_ntop(AF_INET, &addr, ip_str, len)) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Failed to convert IP address to string");
        return PH_NETADDR_ERR_FORMAT;
    }
    
    return PH_NETADDR_SUCCESS;
}

int ph_netaddr_next_ip(uint32_t *current, uint32_t last_ip) {
    if (!current) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "NULL current IP pointer provided");
        return 0;
    }
    
    if (*current >= last_ip) {
        return 0;  // End of range reached
    }
    
    (*current)++;
    return 1;
}

int ph_netaddr_validate_ipv4(const char *ip_str) {
    struct in_addr addr;
    return ip_str && inet_pton(AF_INET, ip_str, &addr) == 1;
}

int ph_netaddr_validate_mac(const char *mac_str) {
    if (!mac_str) {
        return 0;
    }
    
    // Check length
    size_t len = strlen(mac_str);
    if (len != 17) {  // XX:XX:XX:XX:XX:XX = 17 chars
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

int ph_netaddr_validate_cidr(const char *cidr) {
    if (!cidr) {
        return 0;
    }
    
    // Find the prefix separator
    const char *slash = strchr(cidr, '/');
    if (!slash) {
        return 0;
    }
    
    // Validate IP part
    char ip_part[PH_MAX_IP_LEN];
    size_t ip_len = slash - cidr;
    
    if (ip_len >= PH_MAX_IP_LEN) {
        return 0;
    }
    
    memcpy(ip_part, cidr, ip_len);
    ip_part[ip_len] = '\0';
    
    if (!ph_netaddr_validate_ipv4(ip_part)) {
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