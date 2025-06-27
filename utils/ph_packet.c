/**
 * @file ph_packet.c
 * @brief Network packet crafting utility implementation
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#include "nyx_logger.h"
#include "nyx_error.h"
#include "ph_packet.h"

// ICMP echo request/reply constants
#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY 0
#define ICMP_HEADER_LEN 8

// ARP constants
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define ARP_HARDWARE_TYPE 1  // Ethernet
#define ARP_HARDWARE_ADDR_LEN 6  // MAC address length
#define ARP_PROTOCOL_ADDR_LEN 4  // IPv4 address length
#define ARP_PROTOCOL_TYPE 0x0800  // IP

uint16_t ph_packet_checksum(const void *data, size_t len) {
    const uint16_t *buf = (const uint16_t *)data;
    uint32_t sum = 0;

    // Handle odd length by adding a zero byte
    size_t adjusted_len = len;
    uint16_t odd_byte = 0;
    
    if (len % 2 == 1) {
        odd_byte = ((const uint8_t *)data)[len - 1];
        adjusted_len--;
    }

    // Add up 16-bit words
    while (adjusted_len > 1) {
        sum += *buf++;
        adjusted_len -= 2;
    }

    // Add the odd byte if present
    if (len % 2 == 1) {
        sum += odd_byte;
    }

    // Add carry bits
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    // Take one's complement
    return (uint16_t)~sum;
}

int ph_packet_create_icmp_echo(void *buffer, size_t buffer_len, uint16_t id, 
                              uint16_t seq, const void *data, size_t data_len) {
    if (!buffer || buffer_len < ICMP_HEADER_LEN) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid buffer or buffer size too small for ICMP packet");
        return PH_PACKET_ERR_PARAM;
    }

    // Ensure buffer is large enough for header and data
    if (data && data_len > 0 && buffer_len < ICMP_HEADER_LEN + data_len) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Buffer size too small for ICMP packet with data");
        return PH_PACKET_ERR_SIZE;
    }

    struct icmphdr *icmp = (struct icmphdr *)buffer;
    
    // Fill in the ICMP header fields
    memset(icmp, 0, ICMP_HEADER_LEN);
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->un.echo.id = htons(id);
    icmp->un.echo.sequence = htons(seq);
    
    // Copy payload data if provided
    if (data && data_len > 0) {
        memcpy((uint8_t *)buffer + ICMP_HEADER_LEN, data, data_len);
    }
    
    // Calculate the checksum over the whole packet
    size_t packet_len = ICMP_HEADER_LEN + (data ? data_len : 0);
    icmp->checksum = ph_packet_checksum(buffer, packet_len);
    
    return packet_len;
}

int ph_packet_create_arp(void *buffer, size_t buffer_len, 
                        const ph_arp_params_t *params) {
    if (!buffer || buffer_len < sizeof(struct arphdr) + 2*ARP_HARDWARE_ADDR_LEN + 2*ARP_PROTOCOL_ADDR_LEN || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid buffer, buffer size, or ARP parameters");
        return PH_PACKET_ERR_PARAM;
    }

    struct arphdr *arp = (struct arphdr *)buffer;
    
    // Fill in the ARP header fields
    arp->ar_hrd = htons(ARP_HARDWARE_TYPE);  // Ethernet
    arp->ar_pro = htons(ETH_P_IP);           // IPv4
    arp->ar_hln = ARP_HARDWARE_ADDR_LEN;     // MAC address length
    arp->ar_pln = ARP_PROTOCOL_ADDR_LEN;     // IPv4 address length
    arp->ar_op = htons(params->op);          // Operation (request/reply)
    
    // Fill in the ARP payload (addresses)
    uint8_t *ptr = (uint8_t *)(arp + 1);
    
    // Sender hardware address (MAC)
    memcpy(ptr, params->sender_mac, ARP_HARDWARE_ADDR_LEN);
    ptr += ARP_HARDWARE_ADDR_LEN;
    
    // Sender protocol address (IP)
    memcpy(ptr, &params->sender_ip, ARP_PROTOCOL_ADDR_LEN);
    ptr += ARP_PROTOCOL_ADDR_LEN;
    
    // Target hardware address (MAC)
    memcpy(ptr, params->target_mac, ARP_HARDWARE_ADDR_LEN);
    ptr += ARP_HARDWARE_ADDR_LEN;
    
    // Target protocol address (IP)
    memcpy(ptr, &params->target_ip, ARP_PROTOCOL_ADDR_LEN);
    ptr += ARP_PROTOCOL_ADDR_LEN;
    
    return (ptr - (uint8_t *)buffer);
}

int ph_packet_parse_icmp(const void *packet, size_t packet_len, 
                        ph_icmp_params_t *params) {
    if (!packet || packet_len < ICMP_HEADER_LEN || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid packet data, size, or ICMP parameters pointer");
        return PH_PACKET_ERR_PARAM;
    }

    const struct icmphdr *icmp = (const struct icmphdr *)packet;
    
    // Fill in the ICMP parameters
    params->type = icmp->type;
    params->code = icmp->code;
    params->id = ntohs(icmp->un.echo.id);
    params->seq = ntohs(icmp->un.echo.sequence);
    
    // Calculate data length and pointer
    if (packet_len > ICMP_HEADER_LEN) {
        params->data_len = packet_len - ICMP_HEADER_LEN;
        params->data = (const uint8_t *)packet + ICMP_HEADER_LEN;
    } else {
        params->data_len = 0;
        params->data = NULL;
    }
    
    return PH_PACKET_SUCCESS;
}

int ph_packet_parse_arp(const void *packet, size_t packet_len, 
                       ph_arp_params_t *params) {
    const size_t min_size = sizeof(struct arphdr) + 2*ARP_HARDWARE_ADDR_LEN + 2*ARP_PROTOCOL_ADDR_LEN;
    
    if (!packet || packet_len < min_size || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid packet data, size, or ARP parameters pointer");
        return PH_PACKET_ERR_PARAM;
    }

    const struct arphdr *arp = (const struct arphdr *)packet;
    
    // Check if this is an Ethernet/IP ARP packet
    if (ntohs(arp->ar_hrd) != ARP_HARDWARE_TYPE || 
        ntohs(arp->ar_pro) != ETH_P_IP ||
        arp->ar_hln != ARP_HARDWARE_ADDR_LEN ||
        arp->ar_pln != ARP_PROTOCOL_ADDR_LEN) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Invalid ARP packet format");
        return PH_PACKET_ERR_FORMAT;
    }
    
    // Get the operation type
    params->op = ntohs(arp->ar_op);
    
    // Get the addresses
    const uint8_t *ptr = (const uint8_t *)(arp + 1);
    
    // Sender hardware address (MAC)
    memcpy(params->sender_mac, ptr, ARP_HARDWARE_ADDR_LEN);
    ptr += ARP_HARDWARE_ADDR_LEN;
    
    // Sender protocol address (IP)
    memcpy(&params->sender_ip, ptr, ARP_PROTOCOL_ADDR_LEN);
    ptr += ARP_PROTOCOL_ADDR_LEN;
    
    // Target hardware address (MAC)
    memcpy(params->target_mac, ptr, ARP_HARDWARE_ADDR_LEN);
    ptr += ARP_HARDWARE_ADDR_LEN;
    
    // Target protocol address (IP)
    memcpy(&params->target_ip, ptr, ARP_PROTOCOL_ADDR_LEN);
    
    return PH_PACKET_SUCCESS;
}