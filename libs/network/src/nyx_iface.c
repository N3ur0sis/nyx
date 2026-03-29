/**
 * nyx_iface.c - Network Interface Utility Library
 * Author: Neur0sis (2025)
 * Version: 1.0
 *
 * Implementation of common interface manipulation functions for the NYX framework
 */

#define _GNU_SOURCE
/* Header ordering is crucial - include linux headers first */
#include <linux/if.h>
/* Standard C headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>  /* For isalnum() and isxdigit() */
#include <limits.h> /* For PATH_MAX */
/* Directory operations */
#include <dirent.h>
/* Socket and network headers */
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
/* ARP functionality */
#include <net/if_arp.h>

/* Function declaration to avoid implicit declaration warning */
unsigned int if_nametoindex(const char *ifname);

#include "nyx_logger.h"
#include "nyx_iface.h"

// Constants
#define MAC_ADDRESS_LENGTH 17  // XX:XX:XX:XX:XX:XX
#define MAC_BYTES 6
#define SYSFS_NET_PATH "/sys/class/net"
#define PROC_NET_ROUTE "/proc/net/route"

// Helper functions
static int map_error(int err, int default_err) {
    switch (err) {
        case EACCES:
        case EPERM:
            return NYX_IFACE_ERR_PERM;
        case ENOENT:
            return NYX_IFACE_ERR_NOTFOUND;
        case EBUSY:
            return NYX_IFACE_ERR_BUSY;
        case ENOMEM:
        case EMFILE:
        case ENFILE:
            return NYX_IFACE_ERR_SOCKET;
        case EIO:
            return NYX_IFACE_ERR_IO;
        default:
            return default_err;
    }
}

int nyx_iface_is_valid(const char *iface) {
    if (!iface || !*iface) {
        nyx_log(NYX_LOG_ERROR, "Interface validation failed: NULL or empty name");
        return 0;
    }
    
    size_t len = strlen(iface);
    if (len >= IFNAMSIZ) {
        nyx_log(NYX_LOG_ERROR, "Interface validation failed: name too long (%zu >= %d)", 
                len, IFNAMSIZ);
        return 0;
    }
    
    // Validate character set (alphanumeric plus common interface name symbols)
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)iface[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.' && c != ':') {
            nyx_log(NYX_LOG_ERROR, "Interface validation failed: invalid character '%c'", c);
            return 0;
        }
    }
    
    // Verify interface exists in sysfs
    char sysfs_path[PATH_MAX];
    int ret = snprintf(sysfs_path, sizeof(sysfs_path), "%s/%s", SYSFS_NET_PATH, iface);
    if (ret >= (int)sizeof(sysfs_path)) {
        nyx_log(NYX_LOG_ERROR, "Interface validation failed: path too long");
        return 0;
    }
    
    if (access(sysfs_path, F_OK) != 0) {
        nyx_log(NYX_LOG_ERROR, "Interface validation failed: %s not found in sysfs", iface);
        return 0;
    }
    
    return 1;
}

int nyx_iface_is_valid_mac(const char *mac) {
    if (!mac) {
        nyx_log(NYX_LOG_ERROR, "MAC validation failed: NULL pointer");
        return 0;
    }
    
    size_t len = strlen(mac);
    if (len != MAC_ADDRESS_LENGTH) {
        nyx_log(NYX_LOG_ERROR, "MAC validation failed: invalid length %zu", len);
        return 0;
    }
    
    // Validate format: XX:XX:XX:XX:XX:XX
    for (int i = 0; i < MAC_ADDRESS_LENGTH; i++) {
        if ((i + 1) % 3 == 0) {
            // Positions 2, 5, 8, 11, 14 should be colons
            if (mac[i] != ':') {
                nyx_log(NYX_LOG_ERROR, "MAC validation failed: missing colon at position %d", i);
                return 0;
            }
        } else {
            // All other positions should be hex digits
            if (!isxdigit((unsigned char)mac[i])) {
                nyx_log(NYX_LOG_ERROR, "MAC validation failed: invalid hex digit at position %d", i);
                return 0;
            }
        }
    }
    
    return 1;
}

int nyx_iface_is_up(const char *iface) {
    if (!nyx_iface_is_valid(iface)) {
        return 0;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        nyx_log(NYX_LOG_ERROR, "Failed to create socket for interface check: %s", 
                strerror(errno));
        return 0;
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    int result = 0;
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        result = (ifr.ifr_flags & IFF_UP) ? 1 : 0;
    }
    
    close(sock);
    return result;
}

int nyx_iface_get_mac(const char *iface, char *buffer, size_t len) {
    if (!iface || !buffer || len < NYX_IFACE_MAX_MAC_LEN) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    if (!nyx_iface_is_valid(iface)) {
        return NYX_IFACE_ERR_NOTFOUND;
    }
    
    char sysfs_path[PATH_MAX];
    int ret = snprintf(sysfs_path, sizeof(sysfs_path), "%s/%s/address", SYSFS_NET_PATH, iface);
    if (ret >= (int)sizeof(sysfs_path)) {
        nyx_log(NYX_LOG_ERROR, "Path too long for interface %s", iface);
        return NYX_IFACE_ERR_IO;
    }
    
    FILE *fp = fopen(sysfs_path, "r");
    if (!fp) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to open %s: %s", sysfs_path, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    if (!fgets(buffer, (int)len, fp)) {
        int err = errno;
        fclose(fp);
        nyx_log(NYX_LOG_ERROR, "Failed to read MAC from %s: %s", sysfs_path, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    fclose(fp);
    
    // Remove trailing newline and validate
    buffer[strcspn(buffer, "\n\r")] = '\0';
    
    if (!nyx_iface_is_valid_mac(buffer)) {
        nyx_log(NYX_LOG_ERROR, "Invalid MAC format read from sysfs: %s", buffer);
        return NYX_IFACE_ERR_IO;
    }
    
    return NYX_IFACE_SUCCESS;
}

int nyx_iface_get_ipv4(const char *iface, char *buffer, size_t len) {
    if (!iface || !buffer || len < INET_ADDRSTRLEN) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    if (!nyx_iface_is_valid(iface)) {
        return NYX_IFACE_ERR_NOTFOUND;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to create socket: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_SOCKET);
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_addr.sa_family = AF_INET;
    
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        int err = errno;
        close(sock);
        // Don't log as error if interface simply has no IP (common for down interfaces)
        if (err == EADDRNOTAVAIL) {
            nyx_log(NYX_LOG_VERBOSE, "Interface %s has no IPv4 address", iface);
            buffer[0] = '\0';
            return NYX_IFACE_ERR_NOTFOUND;
        }
        nyx_log(NYX_LOG_ERROR, "Failed to get IP address for %s: %s", iface, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    if (!inet_ntop(AF_INET, &sin->sin_addr, buffer, (socklen_t)len)) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to convert IP address: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    close(sock);
    return NYX_IFACE_SUCCESS;
}

int nyx_iface_get_netmask(const char *iface, char *buffer, size_t len) {
    if (!iface || !buffer || len < INET_ADDRSTRLEN) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    if (!nyx_iface_is_valid(iface)) {
        return NYX_IFACE_ERR_NOTFOUND;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to create socket: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_SOCKET);
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    if (ioctl(sock, SIOCGIFNETMASK, &ifr) < 0) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to get netmask for %s: %s", iface, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    if (!inet_ntop(AF_INET, &sin->sin_addr, buffer, (socklen_t)len)) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to convert netmask: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    close(sock);
    return NYX_IFACE_SUCCESS;
}

int nyx_iface_list(char interfaces[][IFNAMSIZ], size_t max_count, size_t *count) {
    if (!interfaces || !count || max_count == 0) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    *count = 0;
    
    DIR *dir = opendir(SYSFS_NET_PATH);
    if (!dir) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to open %s: %s", SYSFS_NET_PATH, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_count) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if it's a valid interface
        if (nyx_iface_is_valid(entry->d_name)) {
            size_t name_len = strlen(entry->d_name);
            size_t copy_len = (name_len < (IFNAMSIZ - 1)) ? name_len : (IFNAMSIZ - 1);
            memcpy(interfaces[*count], entry->d_name, copy_len);
            interfaces[*count][copy_len] = '\0';
            (*count)++;
        }
    }
    
    closedir(dir);
    
    nyx_log(NYX_LOG_INFO, "Found %zu network interfaces", *count);
    return NYX_IFACE_SUCCESS;
}

int nyx_iface_get_mac_by_ip(const char *iface, const char *ip_addr, char *mac_buffer, size_t len) {
    if (!iface || !ip_addr || !mac_buffer || len < NYX_IFACE_MAX_MAC_LEN) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    if (!nyx_iface_is_valid(iface)) {
        return NYX_IFACE_ERR_NOTFOUND;
    }
    
    struct arpreq arpreq;
    memset(&arpreq, 0, sizeof(arpreq));
    
    struct sockaddr_in *sin = (struct sockaddr_in *)&arpreq.arp_pa;
    sin->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_addr, &sin->sin_addr) <= 0) {
        nyx_log(NYX_LOG_ERROR, "Invalid IP address: %s", ip_addr);
        return NYX_IFACE_ERR_PARAM;
    }
    
    strncpy(arpreq.arp_dev, iface, IFNAMSIZ - 1);
    arpreq.arp_dev[IFNAMSIZ - 1] = '\0';
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to create socket: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_SOCKET);
    }
    
    if (ioctl(sock, SIOCGARP, &arpreq) < 0) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "ARP lookup failed for %s on %s: %s", 
                ip_addr, iface, strerror(err));
        return map_error(err, NYX_IFACE_ERR_NOTFOUND);
    }
    
    close(sock);
    
    unsigned char *mac_data = (unsigned char *)&arpreq.arp_ha.sa_data;
    snprintf(mac_buffer, len, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_data[0], mac_data[1], mac_data[2],
             mac_data[3], mac_data[4], mac_data[5]);
    
    return NYX_IFACE_SUCCESS;
}

unsigned int nyx_iface_get_index(const char *iface) {
    if (!nyx_iface_is_valid(iface)) {
        return 0;
    }
    
    return if_nametoindex(iface);
}

int nyx_iface_print_details(void) {
    char interfaces[32][IFNAMSIZ];
    size_t count = 0;
    int result;
    
    result = nyx_iface_list(interfaces, 32, &count);
    if (result != NYX_IFACE_SUCCESS) {
        return result;
    }
    
    printf("Network interfaces:\n");
    printf("------------------\n");
    printf("%-12s %-17s %-15s %-15s %s\n", 
           "Interface", "MAC Address", "IPv4 Address", "Netmask", "Status");
    printf("%-12s %-17s %-15s %-15s %s\n",
           "---------", "-----------", "-----------", "-------", "------");
    
    for (size_t i = 0; i < count; i++) {
        char mac[NYX_IFACE_MAX_MAC_LEN] = {0};
        char ip[INET_ADDRSTRLEN] = {"N/A"};
        char netmask[INET_ADDRSTRLEN] = {"N/A"};
        int is_up = nyx_iface_is_up(interfaces[i]);
        
        nyx_iface_get_mac(interfaces[i], mac, sizeof(mac));
        nyx_iface_get_ipv4(interfaces[i], ip, sizeof(ip));
        nyx_iface_get_netmask(interfaces[i], netmask, sizeof(netmask));
        
        printf("%-12s %-17s %-15s %-15s %s\n",
               interfaces[i], 
               mac,
               ip[0] ? ip : "N/A",
               netmask[0] ? netmask : "N/A",
               is_up ? "UP" : "DOWN");
    }
    
    return NYX_IFACE_SUCCESS;
}

int nyx_iface_get_default_gateway(char *buffer, size_t len) {
    if (!buffer || len < IFNAMSIZ) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    FILE *fp = fopen(PROC_NET_ROUTE, "r");
    if (!fp) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to open %s: %s", PROC_NET_ROUTE, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    char line[256];
    int found = 0;
    
    // Skip header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return NYX_IFACE_ERR_IO;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        char iface[IFNAMSIZ];
        unsigned long dest, gateway;
        int fields;
        
        fields = sscanf(line, "%15s %lx %lx", iface, &dest, &gateway);
        
        if (fields == 3 && dest == 0) {
            strncpy(buffer, iface, len - 1);
            buffer[len - 1] = '\0';
            found = 1;
            break;
        }
    }
    
    fclose(fp);
    return found ? NYX_IFACE_SUCCESS : NYX_IFACE_ERR_NOTFOUND;
}

int nyx_iface_get_gateway_ip(char *buffer, size_t len) {
    if (!buffer || len < INET_ADDRSTRLEN) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    FILE *fp = fopen(PROC_NET_ROUTE, "r");
    if (!fp) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to open %s: %s", PROC_NET_ROUTE, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    char line[256];
    int found = 0;
    
    // Skip header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return NYX_IFACE_ERR_IO;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        char iface[IFNAMSIZ];
        unsigned long dest, gateway;
        int fields;
        
        fields = sscanf(line, "%15s %lx %lx", iface, &dest, &gateway);
        
        if (fields == 3 && dest == 0) {
            struct in_addr gw_addr;
            gw_addr.s_addr = (in_addr_t)gateway;
            
            if (!inet_ntop(AF_INET, &gw_addr, buffer, (socklen_t)len)) {
                int err = errno;
                fclose(fp);
                nyx_log(NYX_LOG_ERROR, "Failed to convert gateway address: %s", strerror(err));
                return map_error(err, NYX_IFACE_ERR_IO);
            }
            
            found = 1;
            break;
        }
    }
    
    fclose(fp);
    return found ? NYX_IFACE_SUCCESS : NYX_IFACE_ERR_NOTFOUND;
}

int nyx_iface_set_status(const char *iface, int up) {
    if (!nyx_iface_is_valid(iface)) {
        return NYX_IFACE_ERR_NOTFOUND;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to create socket: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_SOCKET);
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    // Get current flags
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to get interface flags for %s: %s", 
                iface, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    // Modify flags to set interface up/down
    if (up) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }
    
    // Apply the new flags
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to set interface %s %s: %s", 
                iface, up ? "up" : "down", strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    close(sock);
    nyx_log(NYX_LOG_INFO, "Set interface %s %s", iface, up ? "up" : "down");
    return NYX_IFACE_SUCCESS;
}

int nyx_iface_add_ipv4(const char *iface, const char *ip_addr, int prefix_len) {
    if (!nyx_iface_is_valid(iface) || !ip_addr) {
        return NYX_IFACE_ERR_PARAM;
    }
    
    if (prefix_len < 0 || prefix_len > 32) {
        nyx_log(NYX_LOG_ERROR, "Invalid prefix length: %d", prefix_len);
        return NYX_IFACE_ERR_PARAM;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        int err = errno;
        nyx_log(NYX_LOG_ERROR, "Failed to create socket: %s", strerror(err));
        return map_error(err, NYX_IFACE_ERR_SOCKET);
    }
    
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    
    struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_addr, &sin->sin_addr) <= 0) {
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Invalid IP address: %s", ip_addr);
        return NYX_IFACE_ERR_PARAM;
    }
    
    if (ioctl(sock, SIOCSIFADDR, &ifr) < 0) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to set IP address for %s: %s", 
                iface, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    // Calculate and set netmask based on prefix length
    memset(&ifr.ifr_addr, 0, sizeof(ifr.ifr_addr));
    sin = (struct sockaddr_in *)&ifr.ifr_netmask;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = prefix_len ? htonl((uint32_t)(~((1U << (32 - prefix_len)) - 1U))) : 0;
    
    if (ioctl(sock, SIOCSIFNETMASK, &ifr) < 0) {
        int err = errno;
        close(sock);
        nyx_log(NYX_LOG_ERROR, "Failed to set netmask for %s: %s", 
                iface, strerror(err));
        return map_error(err, NYX_IFACE_ERR_IO);
    }
    
    close(sock);
    nyx_log(NYX_LOG_INFO, "Added IP %s/%d to interface %s", ip_addr, prefix_len, iface);
    return NYX_IFACE_SUCCESS;
}
