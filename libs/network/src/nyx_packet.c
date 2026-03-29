/**
 * @file nyx_packet.c
 * @brief Network packet crafting utility implementation
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#include "nyx_logger.h"
#include "nyx_error.h"
#include "nyx_packet.h"

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

uint16_t nyx_packet_checksum(const void *data, size_t len) {
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

int nyx_packet_create_icmp_echo(void *buffer, size_t buffer_len, uint16_t id, 
                              uint16_t seq, const void *data, size_t data_len) {
    if (!buffer || buffer_len < ICMP_HEADER_LEN) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid buffer or buffer size too small for ICMP packet");
        return NYX_PACKET_ERR_PARAM;
    }

    // Ensure buffer is large enough for header and data
    if (data && data_len > 0 && buffer_len < ICMP_HEADER_LEN + data_len) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Buffer size too small for ICMP packet with data");
        return NYX_PACKET_ERR_SIZE;
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
    
    size_t packet_len = ICMP_HEADER_LEN + (data ? data_len : 0);
    icmp->checksum = nyx_packet_checksum(buffer, packet_len);
    
    return (int)packet_len;
}

int nyx_packet_create_arp(void *buffer, size_t buffer_len, 
                        const nyx_arp_params_t *params) {
    if (!buffer || buffer_len < sizeof(struct arphdr) + 2*ARP_HARDWARE_ADDR_LEN + 2*ARP_PROTOCOL_ADDR_LEN || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid buffer, buffer size, or ARP parameters");
        return NYX_PACKET_ERR_PARAM;
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
    
    return (int)(ptr - (uint8_t *)buffer);
}

int nyx_packet_parse_icmp(const void *packet, size_t packet_len, 
                        nyx_icmp_params_t *params) {
    if (!packet || packet_len < ICMP_HEADER_LEN || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid packet data, size, or ICMP parameters pointer");
        return NYX_PACKET_ERR_PARAM;
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
    
    return NYX_PACKET_SUCCESS;
}

int nyx_packet_parse_arp(const void *packet, size_t packet_len, 
                       nyx_arp_params_t *params) {
    const size_t min_size = sizeof(struct arphdr) + 2*ARP_HARDWARE_ADDR_LEN + 2*ARP_PROTOCOL_ADDR_LEN;
    
    if (!packet || packet_len < min_size || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM, 
                     "Invalid packet data, size, or ARP parameters pointer");
        return NYX_PACKET_ERR_PARAM;
    }

    const struct arphdr *arp = (const struct arphdr *)packet;
    
    // Check if this is an Ethernet/IP ARP packet
    if (ntohs(arp->ar_hrd) != ARP_HARDWARE_TYPE || 
        ntohs(arp->ar_pro) != ETH_P_IP ||
        arp->ar_hln != ARP_HARDWARE_ADDR_LEN ||
        arp->ar_pln != ARP_PROTOCOL_ADDR_LEN) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_GENERIC, 
                     "Invalid ARP packet format");
        return NYX_PACKET_ERR_FORMAT;
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
    
    return NYX_PACKET_SUCCESS;
}

/* ====================================================================
 *  TCP / IPv4 packet crafting and parsing
 * ==================================================================== */

#define IP_HEADER_LEN  20
#define TCP_HEADER_LEN 20

/*
 * IPv4 pseudo-header used for TCP checksum calculation (RFC 793).
 * Packed to ensure no struct padding.
 */
struct tcp_pseudo_header {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed));

uint16_t nyx_packet_tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                                 const void *tcp_seg, size_t tcp_len)
{
    struct tcp_pseudo_header pseudo;
    pseudo.src_ip   = src_ip;
    pseudo.dst_ip   = dst_ip;
    pseudo.zero     = 0;
    pseudo.protocol = IPPROTO_TCP;
    pseudo.tcp_len  = htons((uint16_t)tcp_len);

    size_t total = sizeof(pseudo) + tcp_len;
    uint8_t *buf = malloc(total);
    if (!buf) return 0;

    memcpy(buf, &pseudo, sizeof(pseudo));
    memcpy(buf + sizeof(pseudo), tcp_seg, tcp_len);

    uint16_t cksum = nyx_packet_checksum(buf, total);
    free(buf);
    return cksum;
}

int nyx_packet_create_ip_tcp_syn(void *buffer, size_t buffer_len,
                                 const nyx_tcp_params_t *params)
{
    const size_t total_len = IP_HEADER_LEN + TCP_HEADER_LEN;

    if (!buffer || !params) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM,
                     "NULL buffer or params for TCP SYN packet");
        return NYX_PACKET_ERR_PARAM;
    }
    if (buffer_len < total_len) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM,
                     "Buffer too small for IP+TCP packet (need %zu)", total_len);
        return NYX_PACKET_ERR_SIZE;
    }

    memset(buffer, 0, total_len);

    /* IPv4 header */
    struct iphdr *ip = (struct iphdr *)buffer;
    ip->ihl     = 5;
    ip->version = 4;
    ip->tos     = 0;
    ip->tot_len = htons((uint16_t)total_len);
    ip->id      = htons(54321);
    ip->frag_off = 0;
    ip->ttl     = 64;
    ip->protocol = IPPROTO_TCP;
    ip->check   = 0;
    ip->saddr   = params->src_ip;
    ip->daddr   = params->dst_ip;
    ip->check   = nyx_packet_checksum(ip, IP_HEADER_LEN);

    /* TCP header */
    struct tcphdr *tcp = (struct tcphdr *)((uint8_t *)buffer + IP_HEADER_LEN);
    tcp->source  = htons(params->src_port);
    tcp->dest    = htons(params->dst_port);
    tcp->seq     = htonl(params->seq_num);
    tcp->ack_seq = 0;
    tcp->doff    = 5;
    tcp->fin     = (uint16_t)((params->flags & TH_FIN)  ? 1 : 0);
    tcp->syn     = (uint16_t)((params->flags & TH_SYN)  ? 1 : 0);
    tcp->rst     = (uint16_t)((params->flags & TH_RST)  ? 1 : 0);
    tcp->psh     = (uint16_t)((params->flags & TH_PUSH) ? 1 : 0);
    tcp->ack     = (uint16_t)((params->flags & TH_ACK)  ? 1 : 0);
    tcp->urg     = (uint16_t)((params->flags & TH_URG)  ? 1 : 0);
    tcp->window  = htons(params->window ? params->window : 65535);
    tcp->check   = 0;
    tcp->urg_ptr = 0;

    tcp->check = nyx_packet_tcp_checksum(params->src_ip, params->dst_ip,
                                         tcp, TCP_HEADER_LEN);

    return (int)total_len;
}

int nyx_packet_parse_tcp(const void *packet, size_t packet_len,
                         nyx_tcp_parsed_t *out)
{
    if (!packet || !out) {
        NYX_ERROR_SET(NYX_DOMAIN_CORE, NYX_ERR_PARAM,
                     "NULL packet or output pointer for TCP parse");
        return NYX_PACKET_ERR_PARAM;
    }
    if (packet_len < IP_HEADER_LEN + TCP_HEADER_LEN) {
        return NYX_PACKET_ERR_SIZE;
    }

    const struct iphdr *ip = (const struct iphdr *)packet;
    if (ip->protocol != IPPROTO_TCP)
        return NYX_PACKET_ERR_FORMAT;

    unsigned int ip_hdr_len = (unsigned int)ip->ihl * 4u;
    if (packet_len < ip_hdr_len + TCP_HEADER_LEN)
        return NYX_PACKET_ERR_SIZE;

    const struct tcphdr *tcp =
        (const struct tcphdr *)((const uint8_t *)packet + ip_hdr_len);

    out->src_port = ntohs(tcp->source);
    out->dst_port = ntohs(tcp->dest);
    out->seq_num  = ntohl(tcp->seq);
    out->ack_num  = ntohl(tcp->ack_seq);
    out->flags    = 0;
    if (tcp->syn) out->flags |= TH_SYN;
    if (tcp->ack) out->flags |= TH_ACK;
    if (tcp->rst) out->flags |= TH_RST;
    if (tcp->fin) out->flags |= TH_FIN;
    if (tcp->psh) out->flags |= TH_PUSH;
    if (tcp->urg) out->flags |= TH_URG;

    return NYX_PACKET_SUCCESS;
}
