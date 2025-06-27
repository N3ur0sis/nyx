/**
 * @file ph_packet.h
 * @brief Network packet crafting utility for Nyx framework
 * @author Neur0sis (2025)
 *
 * This module provides functionality for creating and parsing network packets,
 * including ICMP, ARP, and other protocol headers.
 */

#ifndef PH_PACKET_H
#define PH_PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

/**
 * @name Packet Status Codes
 * Return values for packet functions
 * @{
 */
#define PH_PACKET_SUCCESS       0  /**< Operation completed successfully */
#define PH_PACKET_ERR_PARAM    -1  /**< Invalid parameter */
#define PH_PACKET_ERR_SIZE     -2  /**< Buffer size too small */
#define PH_PACKET_ERR_FORMAT   -3  /**< Invalid packet format */
/** @} */

/**
 * IP protocol types
 */
typedef enum {
    PH_PROTO_ICMP = 1,
    PH_PROTO_TCP = 6,
    PH_PROTO_UDP = 17
} ph_protocol_t;

/**
 * ICMP specific parameters
 */
typedef struct {
    uint8_t type;          /**< ICMP type */
    uint8_t code;          /**< ICMP code */
    uint16_t id;           /**< ICMP identifier */
    uint16_t seq;          /**< ICMP sequence */
    size_t data_len;       /**< Length of ICMP data */
    const void *data;      /**< ICMP payload data */
} ph_icmp_params_t;

/**
 * ARP specific parameters
 */
typedef struct {
    uint16_t op;                /**< ARP operation (1=request, 2=reply) */
    uint8_t sender_mac[6];      /**< Sender MAC address */
    uint32_t sender_ip;         /**< Sender IP address (network byte order) */
    uint8_t target_mac[6];      /**< Target MAC address */
    uint32_t target_ip;         /**< Target IP address (network byte order) */
} ph_arp_params_t;

/**
 * Computes standard internet checksum for protocols like IP, ICMP, TCP
 *
 * @param data Data to compute checksum for
 * @param len Length of the data
 * @return Calculated checksum in network byte order
 */
uint16_t ph_packet_checksum(const void *data, size_t len);

/**
 * Creates an ICMP Echo Request (ping) packet
 *
 * @param buffer Buffer to store the constructed packet
 * @param buffer_len Length of the buffer
 * @param id ICMP identifier
 * @param seq ICMP sequence number
 * @param data Optional payload data
 * @param data_len Length of payload data
 * @return Length of the constructed packet on success, or error code on failure
 */
int ph_packet_create_icmp_echo(void *buffer, size_t buffer_len, uint16_t id, 
                              uint16_t seq, const void *data, size_t data_len);

/**
 * Creates an ARP packet
 *
 * @param buffer Buffer to store the constructed packet
 * @param buffer_len Length of the buffer
 * @param params ARP parameters
 * @return Length of the constructed packet on success, or error code on failure
 */
int ph_packet_create_arp(void *buffer, size_t buffer_len, 
                        const ph_arp_params_t *params);

/**
 * Parses an ICMP packet
 *
 * @param packet Packet data to parse
 * @param packet_len Length of the packet data
 * @param params Pointer to store parsed ICMP parameters
 * @return PH_PACKET_SUCCESS on success, or error code on failure
 */
int ph_packet_parse_icmp(const void *packet, size_t packet_len, 
                        ph_icmp_params_t *params);

/**
 * Parses an ARP packet
 *
 * @param packet Packet data to parse
 * @param packet_len Length of the packet data
 * @param params Pointer to store parsed ARP parameters
 * @return PH_PACKET_SUCCESS on success, or error code on failure
 */
int ph_packet_parse_arp(const void *packet, size_t packet_len, 
                       ph_arp_params_t *params);

#endif // PH_PACKET_H