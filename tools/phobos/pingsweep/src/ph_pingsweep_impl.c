/**
 * @file ph_pingsweep_impl.c
 * @brief Implementation of Phobos Ping Sweep API
 * @author Neur0sis (2025)
 *
 * This module performs ICMP-based network discovery by sending Echo
 * Requests to every host in a CIDR range. Multi-threaded scanning
 * splits the host list across a configurable number of workers.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>

#include "nyx_logger.h"
#include "nyx_term.h"
#include "nyx_error.h"
#include "ph_pingsweep_api.h"
#include "nyx_iface.h"
#include "nyx_netaddr.h"
#include "nyx_packet.h"
#include "nyx_socket.h"

#define ICMP_PAYLOAD_SIZE 56
#define RECV_BUF_SIZE     1024

static double timespec_diff_ms(const struct timespec *start,
                               const struct timespec *end)
{
    double s  = (double)(end->tv_sec  - start->tv_sec)  * 1000.0;
    double ns = (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
    return s + ns;
}

int ph_pingsweep_ping_host(const char *ip, int timeout_ms,
                           double *latency_ms)
{
    if (!ip || !latency_ms) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_INVALID_PARAM,
                         NYX_ERROR_SEV_ERROR, "NULL parameter to ping_host",
                         "Provide valid IP and latency pointer");
        return PH_PINGSWEEP_ERR_INVALID_PARAM;
    }

    *latency_ms = 0.0;

    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) != 1)
        return PH_PINGSWEEP_ERR_INVALID_IP;

    int sockfd;
    int ret = nyx_socket_create(NYX_SOCKET_RAW_ICMP, &sockfd);
    if (ret != NYX_SOCKET_SUCCESS)
        return PH_PINGSWEEP_ERR_SOCKET;

    nyx_socket_set_recv_timeout(sockfd, timeout_ms);

    uint16_t id  = (uint16_t)(getpid() & 0xFFFF);
    uint16_t seq = 1;

    uint8_t pkt[ICMP_PAYLOAD_SIZE + 8];
    int pkt_len = nyx_packet_create_icmp_echo(pkt, sizeof(pkt), id, seq,
                                             "NYX", 3);
    if (pkt_len < 0) {
        nyx_socket_close(sockfd);
        return PH_PINGSWEEP_ERR_SEND;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr   = addr;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    ssize_t sent = sendto(sockfd, pkt, (size_t)pkt_len, 0,
                          (struct sockaddr *)&dest, sizeof(dest));
    if (sent < 0) {
        nyx_socket_close(sockfd);
        return PH_PINGSWEEP_ERR_SEND;
    }

    uint8_t recv_buf[RECV_BUF_SIZE];
    int max_attempts = 64;
    while (max_attempts-- > 0) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr *)&from, &from_len);
        if (n < 0) {
            nyx_socket_close(sockfd);
            return PH_PINGSWEEP_ERR_TIMEOUT;
        }

        if (n < (ssize_t)(sizeof(struct iphdr) + 8))
            continue;

        struct iphdr *iph = (struct iphdr *)recv_buf;
        size_t ip_hlen = (size_t)iph->ihl * 4;
        if ((size_t)n < ip_hlen + 8)
            continue;

        struct icmphdr *icmp = (struct icmphdr *)(recv_buf + ip_hlen);

        if (icmp->type == 0 &&
            ntohs(icmp->un.echo.id) == id &&
            ntohs(icmp->un.echo.sequence) == seq) {
            clock_gettime(CLOCK_MONOTONIC, &t_end);
            *latency_ms = timespec_diff_ms(&t_start, &t_end);
            nyx_socket_close(sockfd);
            return PH_PINGSWEEP_SUCCESS;
        }
    }

    nyx_socket_close(sockfd);
    return PH_PINGSWEEP_ERR_TIMEOUT;
}

/**
 * Ping a single host using an existing socket (thread-optimized path).
 * Each thread reuses one socket instead of creating/destroying per-host.
 */
static int ping_host_with_socket(int sockfd, const char *ip, int timeout_ms,
                                 uint16_t echo_id, uint16_t seq,
                                 double *latency_ms)
{
    *latency_ms = 0.0;

    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) != 1)
        return PH_PINGSWEEP_ERR_INVALID_IP;

    uint8_t pkt[ICMP_PAYLOAD_SIZE + 8];
    int pkt_len = nyx_packet_create_icmp_echo(pkt, sizeof(pkt), echo_id, seq,
                                             "NYX", 3);
    if (pkt_len < 0)
        return PH_PINGSWEEP_ERR_SEND;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr   = addr;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    ssize_t sent = sendto(sockfd, pkt, (size_t)pkt_len, 0,
                          (struct sockaddr *)&dest, sizeof(dest));
    if (sent < 0)
        return PH_PINGSWEEP_ERR_SEND;

    (void)timeout_ms;
    uint8_t recv_buf[RECV_BUF_SIZE];
    int max_attempts = 64;
    while (max_attempts-- > 0) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr *)&from, &from_len);
        if (n < 0)
            return PH_PINGSWEEP_ERR_TIMEOUT;

        if (n < (ssize_t)(sizeof(struct iphdr) + 8))
            continue;

        struct iphdr *iph = (struct iphdr *)recv_buf;
        size_t ip_hlen = (size_t)iph->ihl * 4;
        if ((size_t)n < ip_hlen + 8)
            continue;

        struct icmphdr *icmp = (struct icmphdr *)(recv_buf + ip_hlen);

        if (icmp->type == 0 &&
            ntohs(icmp->un.echo.id) == echo_id &&
            ntohs(icmp->un.echo.sequence) == seq) {
            clock_gettime(CLOCK_MONOTONIC, &t_end);
            *latency_ms = timespec_diff_ms(&t_start, &t_end);
            return PH_PINGSWEEP_SUCCESS;
        }
    }

    return PH_PINGSWEEP_ERR_TIMEOUT;
}

typedef struct {
    size_t total;
    atomic_size_t completed;
    atomic_size_t alive;
    pthread_mutex_t lock;
    char last_ip[PH_PINGSWEEP_MAX_IP_LEN];
} sweep_progress_t;

static void sweep_progress_update(sweep_progress_t *progress,
                                  const char *ip, int alive,
                                  double latency_ms)
{
    if (!progress || !ip)
        return;

    size_t completed = atomic_fetch_add(&progress->completed, 1) + 1;
    size_t alive_count = alive
                             ? (atomic_fetch_add(&progress->alive, 1) + 1)
                             : atomic_load(&progress->alive);

    if (nyx_logger_verbose > 0) {
        if (alive)
            nyx_log(NYX_LOG_VERBOSE, "Host %s -> UP (%.2f ms)", ip, latency_ms);
        else
            nyx_log(NYX_LOG_VERBOSE, "Host %s -> DOWN", ip);
    }

    pthread_mutex_lock(&progress->lock);
    snprintf(progress->last_ip, sizeof(progress->last_ip), "%s", ip);
    if (nyx_term_is_interactive()) {
        nyx_term_statusf("[*] Pingsweep %zu/%zu hosts (alive: %zu) current=%s",
                         completed, progress->total, alive_count,
                         progress->last_ip);
    } else if (completed == 1 || completed == progress->total ||
               (completed % 32) == 0) {
        nyx_log(NYX_LOG_INFO,
                "Pingsweep progress: %zu/%zu hosts scanned (alive: %zu)",
                completed, progress->total, alive_count);
    }
    pthread_mutex_unlock(&progress->lock);
}

typedef struct {
    ph_pingsweep_host_t *hosts;
    size_t start_idx;
    size_t count;
    int timeout_ms;
    uint32_t base_ip;
    uint16_t echo_id;      /* unique per thread -- avoids cross-thread reply collision */
    int socket_failed;      /* set to 1 if raw socket creation failed */
    sweep_progress_t *progress;
} sweep_work_t;

static void *sweep_worker(void *arg)
{
    sweep_work_t *w = (sweep_work_t *)arg;

    /* One socket per thread -- avoids create/destroy overhead per host */
    int sockfd;
    int ret = nyx_socket_create(NYX_SOCKET_RAW_ICMP, &sockfd);
    if (ret != NYX_SOCKET_SUCCESS) {
        w->socket_failed = 1;
        for (size_t i = 0; i < w->count; i++)
            w->hosts[w->start_idx + i].alive = 0;
        return NULL;
    }
    nyx_socket_set_recv_timeout(sockfd, w->timeout_ms);

    for (size_t i = 0; i < w->count; i++) {
        uint32_t ip_h = w->base_ip + (uint32_t)(w->start_idx + i);
        char ip_str[PH_PINGSWEEP_MAX_IP_LEN];
        nyx_netaddr_ip_to_str(ip_h, ip_str, sizeof(ip_str));

        snprintf(w->hosts[w->start_idx + i].ip,
                 PH_PINGSWEEP_MAX_IP_LEN, "%s", ip_str);

        double lat = 0.0;
        uint16_t seq = (uint16_t)((i + 1) & 0xFFFF);
        ret = ping_host_with_socket(sockfd, ip_str, w->timeout_ms,
                                    w->echo_id, seq, &lat);

        w->hosts[w->start_idx + i].alive =
            (ret == PH_PINGSWEEP_SUCCESS) ? 1 : 0;
        w->hosts[w->start_idx + i].latency_ms = lat;
        sweep_progress_update(w->progress, ip_str,
                              w->hosts[w->start_idx + i].alive,
                              lat);
    }

    nyx_socket_close(sockfd);
    return NULL;
}

int ph_pingsweep_scan(const ph_pingsweep_config_t *config,
                      ph_pingsweep_result_t **result)
{
    if (!config || !result) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_INVALID_PARAM,
                         NYX_ERROR_SEV_ERROR, "NULL config or result pointer",
                         "Provide valid configuration and result pointer");
        return PH_PINGSWEEP_ERR_INVALID_PARAM;
    }

    if (!config->cidr[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_CIDR,
                         NYX_ERROR_SEV_ERROR, "Empty CIDR in configuration",
                         "Use -c to specify a target (e.g. 192.168.1.0/24)");
        return PH_PINGSWEEP_ERR_CIDR;
    }

    /* Parse CIDR */
    nyx_cidr_info_t cidr;
    if (nyx_netaddr_parse_cidr(config->cidr, &cidr) != NYX_NETADDR_SUCCESS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_CIDR,
                         NYX_ERROR_SEV_ERROR, "Failed to parse CIDR notation",
                         "Format: x.x.x.x/prefix  (e.g. 192.168.1.0/24)");
        return PH_PINGSWEEP_ERR_CIDR;
    }

    if (cidr.num_hosts == 0 || cidr.num_hosts > PH_PINGSWEEP_MAX_HOSTS) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_CIDR,
                         NYX_ERROR_SEV_ERROR,
                         "Host range is empty or too large",
                         "Maximum supported hosts per scan: 65536");
        return PH_PINGSWEEP_ERR_CIDR;
    }

    int timeout = config->timeout_ms > 0
                      ? config->timeout_ms
                      : PH_PINGSWEEP_DEFAULT_TIMEOUT;
    int nthreads = config->threads > 0
                       ? config->threads
                       : PH_PINGSWEEP_DEFAULT_THREADS;
    if ((size_t)nthreads > cidr.num_hosts)
        nthreads = (int)cidr.num_hosts;

    /* Allocate results */
    ph_pingsweep_result_t *res = calloc(1, sizeof(*res));
    if (!res) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_MEMORY,
                         NYX_ERROR_SEV_CRITICAL, "Failed to allocate results",
                         "System may be low on memory");
        return PH_PINGSWEEP_ERR_MEMORY;
    }

    res->hosts = calloc(cidr.num_hosts, sizeof(ph_pingsweep_host_t));
    if (!res->hosts) {
        free(res);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_MEMORY,
                         NYX_ERROR_SEV_CRITICAL,
                         "Failed to allocate host array",
                         "System may be low on memory");
        return PH_PINGSWEEP_ERR_MEMORY;
    }
    res->total = cidr.num_hosts;

    nyx_log(NYX_LOG_INFO, "Scanning %u hosts on %s (%d threads, %d ms timeout)",
            cidr.num_hosts, config->cidr, nthreads, timeout);

    sweep_progress_t progress;
    memset(&progress, 0, sizeof(progress));
    progress.total = cidr.num_hosts;
    pthread_mutex_init(&progress.lock, NULL);

    struct timespec scan_start, scan_end;
    clock_gettime(CLOCK_MONOTONIC, &scan_start);

    /* Divide work among threads */
    pthread_t *threads = calloc((size_t)nthreads, sizeof(pthread_t));
    sweep_work_t *work = calloc((size_t)nthreads, sizeof(sweep_work_t));
    if (!threads || !work) {
        nyx_term_clear_status();
        pthread_mutex_destroy(&progress.lock);
        free(threads);
        free(work);
        free(res->hosts);
        free(res);
        return PH_PINGSWEEP_ERR_MEMORY;
    }

    size_t chunk = cidr.num_hosts / (size_t)nthreads;
    size_t remainder = cidr.num_hosts % (size_t)nthreads;
    size_t offset = 0;

    uint16_t base_id = (uint16_t)(getpid() & 0xFFFF);
    for (int i = 0; i < nthreads; i++) {
        work[i].hosts      = res->hosts;
        work[i].start_idx  = offset;
        work[i].count      = chunk + (i < (int)remainder ? 1 : 0);
        work[i].timeout_ms = timeout;
        work[i].base_ip    = cidr.first_host;
        work[i].echo_id    = (uint16_t)(base_id + (uint16_t)i);
        work[i].progress   = &progress;
        offset += work[i].count;
    }

    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&threads[i], NULL, sweep_worker, &work[i]) != 0) {
            nyx_log(NYX_LOG_ERROR, "Failed to create thread %d", i);
            /* Wait for already-started threads */
            for (int j = 0; j < i; j++)
                pthread_join(threads[j], NULL);
            nyx_term_clear_status();
            pthread_mutex_destroy(&progress.lock);
            free(threads);
            free(work);
            free(res->hosts);
            free(res);
            return PH_PINGSWEEP_ERR_THREAD;
        }
    }

    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    /* Check if ALL threads failed to create sockets */
    int all_socket_failed = 1;
    for (int i = 0; i < nthreads; i++) {
        if (!work[i].socket_failed) {
            all_socket_failed = 0;
            break;
        }
    }

    free(threads);
    free(work);
    nyx_term_clear_status();
    pthread_mutex_destroy(&progress.lock);

    if (all_socket_failed) {
        free(res->hosts);
        free(res);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_SOCKET,
                         NYX_ERROR_SEV_ERROR,
                         "All worker threads failed to create raw ICMP sockets "
                         "(operation not permitted)",
                         "Run with root privileges: sudo nyx-pingsweep ...");
        return PH_PINGSWEEP_ERR_SOCKET;
    }

    clock_gettime(CLOCK_MONOTONIC, &scan_end);
    res->elapsed_ms = timespec_diff_ms(&scan_start, &scan_end);

    /* Count alive hosts */
    res->alive_count = 0;
    for (size_t i = 0; i < res->total; i++) {
        if (res->hosts[i].alive)
            res->alive_count++;
    }

    nyx_log(NYX_LOG_SUCCESS, "Scan complete: %zu/%zu hosts alive (%.1f ms)",
            res->alive_count, res->total, res->elapsed_ms);

    *result = res;
    return PH_PINGSWEEP_SUCCESS;
}

void ph_pingsweep_free_result(ph_pingsweep_result_t *result)
{
    if (!result)
        return;
    free(result->hosts);
    free(result);
}

void ph_pingsweep_print_result(const ph_pingsweep_result_t *result)
{
    if (!result || !result->hosts)
        return;

    printf("\n%-18s %-8s %s\n", "IP Address", "Status", "Latency");
    printf("%-18s %-8s %s\n", "----------", "------", "-------");

    for (size_t i = 0; i < result->total; i++) {
        if (!result->hosts[i].alive)
            continue;
        printf("%-18s %-8s %.2f ms\n",
               result->hosts[i].ip, "UP",
               result->hosts[i].latency_ms);
    }

    printf("\n%zu host(s) alive out of %zu scanned (%.1f ms elapsed)\n",
           result->alive_count, result->total, result->elapsed_ms);
}

int ph_pingsweep_list_interfaces_stdout(void)
{
    return nyx_iface_print_details();
}
