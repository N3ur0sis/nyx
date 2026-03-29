/**
 * @file nyx_socket.h
 * @brief Raw socket utility for Nyx framework
 * @author Neur0sis (2025)
 *
 * Provides functionality for creating and managing raw sockets
 * required for low-level network operations.
 */

#ifndef NYX_SOCKET_H
#define NYX_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>

/**
 * @name Status Codes
 * @{
 */
#define NYX_SOCKET_SUCCESS       0
#define NYX_SOCKET_ERR_CREATE   -1
#define NYX_SOCKET_ERR_BIND     -2
#define NYX_SOCKET_ERR_OPTION   -3
#define NYX_SOCKET_ERR_SEND     -4
#define NYX_SOCKET_ERR_RECV     -5
#define NYX_SOCKET_ERR_PARAM    -6
#define NYX_SOCKET_ERR_TIMEOUT  -7
#define NYX_SOCKET_ERR_PERM     -8
/** @} */

/**
 * @name Socket Types
 * @{
 */
#define NYX_SOCKET_RAW_ICMP     1
#define NYX_SOCKET_RAW_IP       2
#define NYX_SOCKET_RAW_PACKET   3
#define NYX_SOCKET_RAW_TCP      4  /**< AF_INET, SOCK_RAW, IPPROTO_TCP */
/** @} */

#ifdef __cplusplus
extern "C" {
#endif

int nyx_socket_create(int socket_type, int *sockfd);
int nyx_socket_set_recv_timeout(int sockfd, int timeout_ms);
int nyx_socket_set_send_timeout(int sockfd, int timeout_ms);
int nyx_socket_enable_broadcast(int sockfd);
void nyx_socket_close(int sockfd);
int nyx_socket_send(int sockfd, const void *packet, size_t packet_len,
                    const struct sockaddr *dest, socklen_t dest_len);
int nyx_socket_recv(int sockfd, void *buffer, size_t buffer_len,
                    struct sockaddr *src, socklen_t *src_len);

#ifdef __cplusplus
}
#endif

#endif /* NYX_SOCKET_H */
