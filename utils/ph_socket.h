/**
 * @file ph_socket.h
 * @brief Raw socket utility for Nyx framework
 * @author Neur0sis (2025)
 *
 * This module provides functionality for creating and managing raw sockets,
 * which are required for low-level network operations.
 */

#ifndef PH_SOCKET_H
#define PH_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>

/**
 * @name Socket Status Codes
 * Return values for socket functions
 * @{
 */
#define PH_SOCKET_SUCCESS       0  /**< Operation completed successfully */
#define PH_SOCKET_ERR_CREATE   -1  /**< Failed to create socket */
#define PH_SOCKET_ERR_BIND     -2  /**< Failed to bind socket */
#define PH_SOCKET_ERR_OPTION   -3  /**< Failed to set socket option */
#define PH_SOCKET_ERR_SEND     -4  /**< Failed to send packet */
#define PH_SOCKET_ERR_RECV     -5  /**< Failed to receive packet */
#define PH_SOCKET_ERR_PARAM    -6  /**< Invalid parameter */
#define PH_SOCKET_ERR_TIMEOUT  -7  /**< Operation timed out */
#define PH_SOCKET_ERR_PERM     -8  /**< Permission denied */
/** @} */

/**
 * @name Socket Types
 * Types of raw sockets that can be created
 * @{
 */
#define PH_SOCKET_RAW_ICMP     1  /**< Raw ICMP socket */
#define PH_SOCKET_RAW_IP       2  /**< Raw IP socket (with IP header) */
#define PH_SOCKET_RAW_PACKET   3  /**< Link-layer raw packet socket */
/** @} */

/**
 * Creates a raw socket of the specified type
 *
 * @param socket_type Type of socket to create (PH_SOCKET_RAW_*)
 * @param sockfd Pointer to store the socket file descriptor
 * @return PH_SOCKET_SUCCESS on success, or error code on failure
 */
int ph_socket_create(int socket_type, int *sockfd);

/**
 * Sets receive timeout on a socket
 *
 * @param sockfd Socket file descriptor
 * @param timeout_ms Timeout in milliseconds
 * @return PH_SOCKET_SUCCESS on success, or error code on failure
 */
int ph_socket_set_recv_timeout(int sockfd, int timeout_ms);

/**
 * Sets send timeout on a socket
 *
 * @param sockfd Socket file descriptor
 * @param timeout_ms Timeout in milliseconds
 * @return PH_SOCKET_SUCCESS on success, or error code on failure
 */
int ph_socket_set_send_timeout(int sockfd, int timeout_ms);

/**
 * Enables broadcast on a socket (for subnet broadcasts)
 *
 * @param sockfd Socket file descriptor
 * @return PH_SOCKET_SUCCESS on success, or error code on failure
 */
int ph_socket_enable_broadcast(int sockfd);

/**
 * Closes a socket
 *
 * @param sockfd Socket file descriptor to close
 */
void ph_socket_close(int sockfd);

/**
 * Sends a raw packet through a socket
 *
 * @param sockfd Socket file descriptor
 * @param packet Packet data to send
 * @param packet_len Length of the packet data
 * @param dest Destination address
 * @return Number of bytes sent on success, or error code on failure
 */
int ph_socket_send(int sockfd, const void *packet, size_t packet_len, 
                  const struct sockaddr *dest);

/**
 * Receives a raw packet from a socket
 *
 * @param sockfd Socket file descriptor
 * @param buffer Buffer to store received data
 * @param buffer_len Length of the buffer
 * @param src Pointer to store source address
 * @param src_len Pointer to store source address length
 * @return Number of bytes received on success, or error code on failure
 */
int ph_socket_recv(int sockfd, void *buffer, size_t buffer_len, 
                  struct sockaddr *src, socklen_t *src_len);

#endif // PH_SOCKET_H