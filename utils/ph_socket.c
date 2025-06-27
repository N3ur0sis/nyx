/**
 * @file ph_socket.c
 * @brief Raw socket utility implementation
 * @author Neur0sis (2025)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>

#include "nyx_logger.h"
#include "nyx_error.h"
#include "ph_socket.h"

int ph_socket_create(int socket_type, int *sockfd) {
    if (!sockfd) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "NULL sockfd pointer provided");
        return PH_SOCKET_ERR_PARAM;
    }

    int domain = AF_INET;  // IPv4
    int type = SOCK_RAW;
    int protocol = 0;

    switch (socket_type) {
        case PH_SOCKET_RAW_ICMP:
            protocol = IPPROTO_ICMP;
            break;
        case PH_SOCKET_RAW_IP:
            protocol = IPPROTO_RAW;
            break;
        case PH_SOCKET_RAW_PACKET:
            domain = AF_PACKET;
            protocol = htons(ETH_P_ALL);
            break;
        default:
            NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                         "Invalid socket type: %d", socket_type);
            return PH_SOCKET_ERR_PARAM;
    }

    int fd = socket(domain, type, protocol);
    if (fd < 0) {
        int err = errno;
        if (err == EPERM || err == EACCES) {
            NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PERMISSION, 
                         "Permission denied creating raw socket (requires root/sudo)");
            return PH_SOCKET_ERR_PERM;
        } else {
            NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                         "Failed to create socket: %s", strerror(err));
            return PH_SOCKET_ERR_CREATE;
        }
    }

    *sockfd = fd;
    return PH_SOCKET_SUCCESS;
}

int ph_socket_set_recv_timeout(int sockfd, int timeout_ms) {
    if (sockfd < 0 || timeout_ms < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid socket descriptor or timeout value");
        return PH_SOCKET_ERR_PARAM;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Failed to set receive timeout: %s", strerror(errno));
        return PH_SOCKET_ERR_OPTION;
    }

    return PH_SOCKET_SUCCESS;
}

int ph_socket_set_send_timeout(int sockfd, int timeout_ms) {
    if (sockfd < 0 || timeout_ms < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid socket descriptor or timeout value");
        return PH_SOCKET_ERR_PARAM;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Failed to set send timeout: %s", strerror(errno));
        return PH_SOCKET_ERR_OPTION;
    }

    return PH_SOCKET_SUCCESS;
}

int ph_socket_enable_broadcast(int sockfd) {
    if (sockfd < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid socket descriptor");
        return PH_SOCKET_ERR_PARAM;
    }

    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Failed to enable broadcast: %s", strerror(errno));
        return PH_SOCKET_ERR_OPTION;
    }

    return PH_SOCKET_SUCCESS;
}

void ph_socket_close(int sockfd) {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

int ph_socket_send(int sockfd, const void *packet, size_t packet_len, 
                  const struct sockaddr *dest) {
    if (sockfd < 0 || !packet || packet_len == 0 || !dest) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid parameters for socket send operation");
        return PH_SOCKET_ERR_PARAM;
    }

    ssize_t sent = sendto(sockfd, packet, packet_len, 0, dest, sizeof(struct sockaddr));
    
    if (sent < 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Failed to send packet: %s", strerror(errno));
        return PH_SOCKET_ERR_SEND;
    }

    return (int)sent;
}

int ph_socket_recv(int sockfd, void *buffer, size_t buffer_len, 
                  struct sockaddr *src, socklen_t *src_len) {
    if (sockfd < 0 || !buffer || buffer_len == 0) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid parameters for socket receive operation");
        return PH_SOCKET_ERR_PARAM;
    }

    ssize_t received = recvfrom(sockfd, buffer, buffer_len, 0, src, src_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Timeout occurred, not an error
            return PH_SOCKET_ERR_TIMEOUT;
        }
        
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Failed to receive packet: %s", strerror(errno));
        return PH_SOCKET_ERR_RECV;
    }

    return (int)received;
}