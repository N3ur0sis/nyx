/**
 * @file nyx_packet.h
 * @brief Network packet crafting utility for Nyx framework
 * @author Neur0sis (2025)
 *
 * Provides functionality for creating and parsing network packets,
 * including ICMP, ARP, and other protocol headers.
 */

#ifndef NYX_PACKET_H
#define NYX_PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

/**
 * @name Status Codes
 * @{
 */
#define NYX_PACKET_SUCCESS       0
#define NYX_PACKET_ERR_PARAM    -1
#define NYX_PACKET_ERR_SIZE     -2
#define NYX_PACKET_ERR_FORMAT   -3
/** @} */

typedef enum {
    NYX_PROTO_ICMP = 1,
    NYX_PROTO_TCP  = 6,
    NYX_PROTO_UDP  = 17
} nyx_protocol_t;

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t id;
    uint16_t seq;
    size_t data_len;
    const void *data;
} nyx_icmp_params_t;

typedef struct {
    uint16_t op;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} nyx_arp_params_t;

/**
 * TCP packet construction parameters
 */
typedef struct {
    uint32_t src_ip;     /**< Source IP (network byte order) */
    uint32_t dst_ip;     /**< Destination IP (network byte order) */
    uint16_t src_port;   /**< Source port (host byte order) */
    uint16_t dst_port;   /**< Destination port (host byte order) */
    uint32_t seq_num;    /**< Sequence number (host byte order) */
    uint8_t  flags;      /**< TCP flags (TH_SYN, TH_ACK, TH_RST, etc.) */
    uint16_t window;     /**< Window size (host byte order) */
} nyx_tcp_params_t;

/**
 * Parsed TCP packet fields
 */
typedef struct {
    uint16_t src_port;   /**< Source port (host byte order) */
    uint16_t dst_port;   /**< Destination port (host byte order) */
    uint32_t seq_num;    /**< Sequence number (host byte order) */
    uint32_t ack_num;    /**< Acknowledgment number (host byte order) */
    uint8_t  flags;      /**< TCP flags */
} nyx_tcp_parsed_t;

#ifdef __cplusplus
extern "C" {
#endif

uint16_t nyx_packet_checksum(const void *data, size_t len);
int nyx_packet_create_icmp_echo(void *buffer, size_t buffer_len,
                                uint16_t id, uint16_t seq,
                                const void *data, size_t data_len);
int nyx_packet_create_arp(void *buffer, size_t buffer_len,
                          const nyx_arp_params_t *params);
int nyx_packet_parse_icmp(const void *packet, size_t packet_len,
                          nyx_icmp_params_t *params);
int nyx_packet_parse_arp(const void *packet, size_t packet_len,
                         nyx_arp_params_t *params);

/**
 * Builds a complete IPv4 + TCP SYN packet.
 * @param buffer     Output buffer (must be >= 40 bytes)
 * @param buffer_len Size of buffer
 * @param params     TCP parameters (IPs, ports, seq, flags, window)
 * @return Total packet length on success, or negative error code
 */
int nyx_packet_create_ip_tcp_syn(void *buffer, size_t buffer_len,
                                 const nyx_tcp_params_t *params);

/**
 * Computes TCP checksum including the IPv4 pseudo-header.
 * @param src_ip   Source IP (network byte order)
 * @param dst_ip   Destination IP (network byte order)
 * @param tcp_seg  Pointer to TCP segment (header + data)
 * @param tcp_len  Length of TCP segment in bytes
 * @return Checksum value in network byte order
 */
uint16_t nyx_packet_tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                                 const void *tcp_seg, size_t tcp_len);

/**
 * Parses an IPv4+TCP packet, extracting TCP fields.
 * @param packet     Raw packet starting at IPv4 header
 * @param packet_len Total length of raw packet
 * @param out        Parsed TCP fields output
 * @return NYX_PACKET_SUCCESS or negative error code
 */
int nyx_packet_parse_tcp(const void *packet, size_t packet_len,
                         nyx_tcp_parsed_t *out);

#ifdef __cplusplus
}
#endif

#endif /* NYX_PACKET_H */
