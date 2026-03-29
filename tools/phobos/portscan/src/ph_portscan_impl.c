/**
 * @file ph_portscan_impl.c
 * @brief Implementation of Phobos Port Scan API
 * @author Neur0sis (2025)
 *
 * TCP port scanner supporting two modes:
 *   - TCP Connect: uses connect() with non-blocking + poll()
 *   - TCP SYN (half-open): sends raw SYN packets via nyx_packet
 *
 * Multi-threaded scanning splits the port list across workers.
 * Auto-fallback from SYN to Connect when not running as root.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "nyx_logger.h"
#include "nyx_term.h"
#include "nyx_error.h"
#include "nyx_packet.h"
#include "nyx_socket.h"
#include "ph_portscan_api.h"

/* ====================================================================
 *  Top 100 most common TCP ports (nmap frequency data)
 * ==================================================================== */

static const uint16_t TOP_PORTS_100[] = {
    80,    23,    443,   21,   22,   25,   3389, 110,  445,   139,  143,  53,   135,   3306, 8080,
    1723,  111,   995,   993,  5900, 1025, 587,  8888, 199,   1720, 465,  548,  113,   81,   6001,
    10000, 514,   5060,  179,  1026, 2000, 8443, 8000, 32768, 554,  26,   1433, 49152, 2001, 515,
    8008,  49154, 1027,  5666, 646,  5000, 5631, 631,  49153, 8081, 2049, 88,   79,    5800, 106,
    2121,  1110,  49155, 6000, 513,  990,  5357, 427,  49156, 543,  544,  5101, 144,   7,    389,
    8009,  3128,  444,   9999, 5009, 7070, 5190, 3000, 5432,  1900, 3986, 13,   1029,  9,    5051,
    6646,  49157, 1028,  873,  1755, 2717, 4899, 9100, 119,   37};

#define TOP_PORTS_COUNT ((int)(sizeof(TOP_PORTS_100) / sizeof(TOP_PORTS_100[0])))

/* ====================================================================
 *  Utility helpers
 * ==================================================================== */

static double timespec_diff_ms(const struct timespec *start, const struct timespec *end)
{
    double s = (double)(end->tv_sec - start->tv_sec) * 1000.0;
    double ns = (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
    return s + ns;
}

/**
 * Resolve the local source IP that routes to @dst_ip.
 * Uses the connected-UDP-socket trick (no actual traffic).
 * Returns the source IP in network byte order, or 0 on failure.
 */
static uint32_t resolve_local_ip(uint32_t dst_ip)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0;

    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(53);
    dst.sin_addr.s_addr = dst_ip;

    if (connect(fd, (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(fd);
        return 0;
    }

    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&local, &len) < 0) {
        close(fd);
        return 0;
    }

    close(fd);
    return local.sin_addr.s_addr;
}

/**
 * Read a random uint16 from /dev/urandom.
 */
static uint16_t random_port(void)
{
    uint16_t val = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, &val, sizeof(val));
        (void)n;
        close(fd);
    }
    if (val < 1024)
        val += 1024;
    return val;
}

static uint32_t random_seq(void)
{
    uint32_t val = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, &val, sizeof(val));
        (void)n;
        close(fd);
    }
    return val;
}

static int can_use_syn_mode(int timeout_ms)
{
    int send_fd = -1;
    int recv_fd = -1;

    if (nyx_socket_create(NYX_SOCKET_RAW_IP, &send_fd) != 0)
        return 0;

    int one = 1;
    (void)setsockopt(send_fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    if (nyx_socket_create(NYX_SOCKET_RAW_TCP, &recv_fd) != 0) {
        nyx_socket_close(send_fd);
        return 0;
    }

    nyx_socket_set_recv_timeout(recv_fd, timeout_ms);
    nyx_socket_close(send_fd);
    nyx_socket_close(recv_fd);
    return 1;
}

/* ====================================================================
 *  Build the port list from config
 * ==================================================================== */

/**
 * Allocates and fills a port array based on config.
 * @param config  Scan configuration
 * @param ports   Output pointer to allocated uint16_t array
 * @param count   Output number of ports
 * @return 0 on success, negative on error
 */
static int build_port_list(const ph_portscan_config_t *config, uint16_t **ports, size_t *count)
{
    if (config->top_ports > 0) {
        int n = config->top_ports;
        if (n > TOP_PORTS_COUNT)
            n = TOP_PORTS_COUNT;

        uint16_t *arr = malloc(sizeof(uint16_t) * (size_t)n);
        if (!arr)
            return PH_PORTSCAN_ERR_MEMORY;
        memcpy(arr, TOP_PORTS_100, sizeof(uint16_t) * (size_t)n);
        *ports = arr;
        *count = (size_t)n;
        return 0;
    }

    uint16_t start = config->port_start;
    uint16_t end = config->port_end;
    if (start == 0 && end == 0) {
        start = 1;
        end = 1024;
    }
    if (start > end) {
        uint16_t tmp = start;
        start = end;
        end = tmp;
    }

    size_t n = (size_t)(end - start) + 1;
    uint16_t *arr = malloc(sizeof(uint16_t) * n);
    if (!arr)
        return PH_PORTSCAN_ERR_MEMORY;

    for (size_t i = 0; i < n; i++)
        arr[i] = (uint16_t)(start + (uint16_t)i);

    *ports = arr;
    *count = n;
    return 0;
}

/* ====================================================================
 *  TCP Connect scan
 * ==================================================================== */

static ph_port_state_t connect_scan_port(const char *target, uint16_t port, int timeout_ms)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return PH_PORT_FILTERED;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return PH_PORT_FILTERED;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, target, &addr.sin_addr);

    int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == 0) {
        close(fd);
        return PH_PORT_OPEN;
    }
    if (errno != EINPROGRESS) {
        close(fd);
        return (errno == ECONNREFUSED) ? PH_PORT_CLOSED : PH_PORT_FILTERED;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;

    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0) {
        close(fd);
        return PH_PORT_FILTERED;
    }

    int so_err = 0;
    socklen_t so_len = sizeof(so_err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &so_len) != 0) {
        close(fd);
        return PH_PORT_FILTERED;
    }
    close(fd);

    if (so_err == 0)
        return PH_PORT_OPEN;
    if (so_err == ECONNREFUSED)
        return PH_PORT_CLOSED;
    return PH_PORT_FILTERED;
}

/* ====================================================================
 *  TCP SYN scan
 * ==================================================================== */

static ph_port_state_t syn_scan_port(uint32_t src_ip, uint32_t dst_ip, uint16_t port, int send_fd,
                                     int recv_fd, int timeout_ms)
{
    uint16_t src_port = random_port();
    uint32_t seq = random_seq();

    nyx_tcp_params_t params;
    memset(&params, 0, sizeof(params));
    params.src_ip = src_ip;
    params.dst_ip = dst_ip;
    params.src_port = src_port;
    params.dst_port = port;
    params.seq_num = seq;
    params.flags = TH_SYN;
    params.window = 65535;

    uint8_t pkt[64];
    int pkt_len = nyx_packet_create_ip_tcp_syn(pkt, sizeof(pkt), &params);
    if (pkt_len < 0)
        return PH_PORT_FILTERED;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = dst_ip;

    if (sendto(send_fd, pkt, (size_t)pkt_len, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        return PH_PORT_FILTERED;
    }

    struct pollfd pfd;
    pfd.fd = recv_fd;
    pfd.events = POLLIN;

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int remaining = (int)((deadline.tv_sec - now.tv_sec) * 1000 +
                              (deadline.tv_nsec - now.tv_nsec) / 1000000);
        if (remaining <= 0)
            break;

        int pr = poll(&pfd, 1, remaining);
        if (pr <= 0)
            break;

        uint8_t recv_buf[128];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        ssize_t n =
            recvfrom(recv_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from, &from_len);
        if (n < 40)
            continue;

        if (from.sin_addr.s_addr != dst_ip)
            continue;

        nyx_tcp_parsed_t parsed;
        if (nyx_packet_parse_tcp(recv_buf, (size_t)n, &parsed) != 0)
            continue;

        if (parsed.src_port != port || parsed.dst_port != src_port)
            continue;

        if ((parsed.flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)) {
            /* Send RST to clean up the half-open connection */
            nyx_tcp_params_t rst;
            memset(&rst, 0, sizeof(rst));
            rst.src_ip = src_ip;
            rst.dst_ip = dst_ip;
            rst.src_port = src_port;
            rst.dst_port = port;
            rst.seq_num = seq + 1;
            rst.flags = TH_RST;
            uint8_t rst_pkt[64];
            int rst_len = nyx_packet_create_ip_tcp_syn(rst_pkt, sizeof(rst_pkt), &rst);
            if (rst_len > 0) {
                sendto(send_fd, rst_pkt, (size_t)rst_len, 0, (struct sockaddr *)&dest,
                       sizeof(dest));
            }
            return PH_PORT_OPEN;
        }

        if (parsed.flags & TH_RST)
            return PH_PORT_CLOSED;
    }

    return PH_PORT_FILTERED;
}

/* ====================================================================
 *  Thread worker
 * ==================================================================== */

typedef struct {
    size_t total;
    atomic_size_t completed;
    atomic_size_t open_count;
    pthread_mutex_t lock;
    uint16_t last_port;
} scan_progress_t;

static void scan_progress_update(scan_progress_t *progress, uint16_t port, ph_port_state_t state)
{
    if (!progress)
        return;

    size_t completed = atomic_fetch_add(&progress->completed, 1) + 1;
    size_t open_count = (state == PH_PORT_OPEN) ? (atomic_fetch_add(&progress->open_count, 1) + 1)
                                                : atomic_load(&progress->open_count);

    if (state == PH_PORT_OPEN)
        nyx_log(NYX_LOG_INFO, "Open port discovered: %u/tcp", port);

    if (nyx_logger_verbose > 0) {
        const char *state_str = (state == PH_PORT_OPEN)     ? "open"
                                : (state == PH_PORT_CLOSED) ? "closed"
                                                            : "filtered";
        nyx_log(NYX_LOG_VERBOSE, "Port %u/tcp -> %s", port, state_str);
    }

    pthread_mutex_lock(&progress->lock);
    progress->last_port = port;
    if (nyx_term_is_interactive()) {
        nyx_term_statusf("[*] Portscan %zu/%zu ports (open: %zu) current=%u/tcp", completed,
                         progress->total, open_count, (unsigned)progress->last_port);
    } else if (completed == 1 || completed == progress->total || (completed % 64) == 0) {
        nyx_log(NYX_LOG_INFO, "Portscan progress: %zu/%zu ports scanned (open: %zu)", completed,
                progress->total, open_count);
    }
    pthread_mutex_unlock(&progress->lock);
}

typedef struct {
    ph_portscan_port_t *results; /**< Shared results array */
    const uint16_t *ports;       /**< Port list to scan */
    size_t start_idx;            /**< First index in ports[] for this thread */
    size_t count;                /**< Number of ports for this thread */
    const char *target;          /**< Target IP string */
    uint32_t target_nbo;         /**< Target IP, network byte order */
    uint32_t src_ip;             /**< Local IP, network byte order (SYN only) */
    ph_portscan_mode_t mode;
    int timeout_ms;
    scan_progress_t *progress;
} scan_work_t;

static void *scan_worker(void *arg)
{
    scan_work_t *w = (scan_work_t *)arg;

    int send_fd = -1;
    int recv_fd = -1;

    if (w->mode == PH_PORTSCAN_TCP_SYN) {
        if (nyx_socket_create(NYX_SOCKET_RAW_IP, &send_fd) != 0) {
            for (size_t i = 0; i < w->count; i++) {
                w->results[w->start_idx + i].port = w->ports[w->start_idx + i];
                w->results[w->start_idx + i].state = PH_PORT_FILTERED;
            }
            return NULL;
        }
        int one = 1;
        setsockopt(send_fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

        if (nyx_socket_create(NYX_SOCKET_RAW_TCP, &recv_fd) != 0) {
            nyx_socket_close(send_fd);
            for (size_t i = 0; i < w->count; i++) {
                w->results[w->start_idx + i].port = w->ports[w->start_idx + i];
                w->results[w->start_idx + i].state = PH_PORT_FILTERED;
            }
            return NULL;
        }
        nyx_socket_set_recv_timeout(recv_fd, w->timeout_ms);
    }

    for (size_t i = 0; i < w->count; i++) {
        size_t idx = w->start_idx + i;
        uint16_t port = w->ports[idx];
        w->results[idx].port = port;

        if (w->mode == PH_PORTSCAN_TCP_SYN) {
            w->results[idx].state =
                syn_scan_port(w->src_ip, w->target_nbo, port, send_fd, recv_fd, w->timeout_ms);
        } else {
            w->results[idx].state = connect_scan_port(w->target, port, w->timeout_ms);
        }

        scan_progress_update(w->progress, port, w->results[idx].state);
    }

    if (send_fd >= 0)
        nyx_socket_close(send_fd);
    if (recv_fd >= 0)
        nyx_socket_close(recv_fd);
    return NULL;
}

/* ====================================================================
 *  Public API
 * ==================================================================== */

int ph_portscan_scan(const ph_portscan_config_t *config, ph_portscan_result_t **result)
{
    if (!config || !result) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "NULL config or result pointer",
                         "Provide valid configuration and result pointer");
        return PH_PORTSCAN_ERR_INVALID_PARAM;
    }

    if (!config->target[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_INVALID_TARGET, NYX_ERROR_SEV_ERROR,
                         "Empty target in configuration", "Use -t to specify a target IP address");
        return PH_PORTSCAN_ERR_INVALID_TARGET;
    }

    struct in_addr target_addr;
    if (inet_pton(AF_INET, config->target, &target_addr) != 1) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_INVALID_TARGET, NYX_ERROR_SEV_ERROR,
                         "Invalid target IP address",
                         "Use a valid IPv4 address (e.g. 192.168.1.5)");
        return PH_PORTSCAN_ERR_INVALID_TARGET;
    }

    ph_portscan_mode_t mode = config->mode;
    if (mode == PH_PORTSCAN_TCP_SYN && geteuid() != 0) {
        nyx_log(NYX_LOG_WARN, "SYN scan requires root -- falling back to TCP Connect");
        mode = PH_PORTSCAN_TCP_CONNECT;
    }

    int timeout = config->timeout_ms > 0 ? config->timeout_ms : PH_PORTSCAN_DEFAULT_TIMEOUT;
    int nthreads = config->threads > 0 ? config->threads : PH_PORTSCAN_DEFAULT_THREADS;

    /* Build port list */
    uint16_t *port_list = NULL;
    size_t port_count = 0;
    int ret = build_port_list(config, &port_list, &port_count);
    if (ret != 0) {
        free(port_list);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_MEMORY, NYX_ERROR_SEV_ERROR,
                         "Memory allocation failed for port list", "Reduce port range");
        return ret;
    }
    if (port_count == 0) {
        free(port_list);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "No ports to scan", "Specify a port range (-p) or top-ports count (-P)");
        return PH_PORTSCAN_ERR_INVALID_PARAM;
    }

    if ((size_t)nthreads > port_count)
        nthreads = (int)port_count;

    /* Resolve source IP for SYN mode */
    uint32_t src_ip = 0;
    if (mode == PH_PORTSCAN_TCP_SYN) {
        src_ip = resolve_local_ip(target_addr.s_addr);
        if (src_ip == 0) {
            nyx_log(NYX_LOG_WARN, "Cannot resolve local IP -- falling back to TCP Connect");
            mode = PH_PORTSCAN_TCP_CONNECT;
        } else if (!can_use_syn_mode(timeout)) {
            nyx_log(NYX_LOG_WARN, "Raw TCP sockets unavailable -- falling back to TCP Connect");
            mode = PH_PORTSCAN_TCP_CONNECT;
        }
    }

    /* Allocate results */
    ph_portscan_result_t *res = calloc(1, sizeof(*res));
    if (!res) {
        free(port_list);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_MEMORY, NYX_ERROR_SEV_CRITICAL,
                         "Failed to allocate results", "System may be low on memory");
        return PH_PORTSCAN_ERR_MEMORY;
    }

    res->ports = calloc(port_count, sizeof(ph_portscan_port_t));
    if (!res->ports) {
        free(res);
        free(port_list);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_MEMORY, NYX_ERROR_SEV_CRITICAL,
                         "Failed to allocate port array", "System may be low on memory");
        return PH_PORTSCAN_ERR_MEMORY;
    }
    snprintf(res->target, sizeof(res->target), "%s", config->target);
    res->scanned_count = port_count;
    res->actual_mode = mode;

    const char *mode_str = (mode == PH_PORTSCAN_TCP_SYN) ? "SYN" : "Connect";
    nyx_log(NYX_LOG_INFO, "Scanning %zu ports on %s [%s mode, %d threads, %d ms timeout]",
            port_count, config->target, mode_str, nthreads, timeout);

    scan_progress_t progress;
    memset(&progress, 0, sizeof(progress));
    progress.total = port_count;
    pthread_mutex_init(&progress.lock, NULL);

    struct timespec scan_start, scan_end;
    clock_gettime(CLOCK_MONOTONIC, &scan_start);

    /* Divide work among threads */
    pthread_t *threads = calloc((size_t)nthreads, sizeof(pthread_t));
    scan_work_t *work = calloc((size_t)nthreads, sizeof(scan_work_t));
    if (!threads || !work) {
        nyx_term_clear_status();
        pthread_mutex_destroy(&progress.lock);
        free(threads);
        free(work);
        free(res->ports);
        free(res);
        free(port_list);
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_MEMORY, NYX_ERROR_SEV_ERROR,
                         "Memory allocation failed for scan worker pool",
                         "Reduce the number of threads or ports being scanned");
        return PH_PORTSCAN_ERR_MEMORY;
    }

    size_t chunk = port_count / (size_t)nthreads;
    size_t remainder = port_count % (size_t)nthreads;
    size_t offset = 0;

    for (int i = 0; i < nthreads; i++) {
        work[i].results = res->ports;
        work[i].ports = port_list;
        work[i].start_idx = offset;
        work[i].count = chunk + ((size_t)i < remainder ? 1 : 0);
        work[i].target = config->target;
        work[i].target_nbo = target_addr.s_addr;
        work[i].src_ip = src_ip;
        work[i].mode = mode;
        work[i].timeout_ms = timeout;
        work[i].progress = &progress;
        offset += work[i].count;
    }

    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&threads[i], NULL, scan_worker, &work[i]) != 0) {
            nyx_log(NYX_LOG_ERROR, "Failed to create thread %d", i);
            for (int j = 0; j < i; j++)
                pthread_join(threads[j], NULL);
            nyx_term_clear_status();
            pthread_mutex_destroy(&progress.lock);
            free(threads);
            free(work);
            free(res->ports);
            free(res);
            free(port_list);
            NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_THREAD, NYX_ERROR_SEV_ERROR,
                             "Failed to create scan worker thread",
                             "Reduce thread count with -T flag or check system thread limits");
            return PH_PORTSCAN_ERR_THREAD;
        }
    }

    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    free(threads);
    free(work);
    free(port_list);
    nyx_term_clear_status();
    pthread_mutex_destroy(&progress.lock);

    clock_gettime(CLOCK_MONOTONIC, &scan_end);
    res->elapsed_ms = timespec_diff_ms(&scan_start, &scan_end);

    res->open_count = 0;
    for (size_t i = 0; i < res->scanned_count; i++) {
        if (res->ports[i].state == PH_PORT_OPEN)
            res->open_count++;
    }

    nyx_log(NYX_LOG_SUCCESS, "Scan complete: %zu open / %zu scanned (%.1f ms)", res->open_count,
            res->scanned_count, res->elapsed_ms);

    *result = res;
    return PH_PORTSCAN_SUCCESS;
}

void ph_portscan_free_result(ph_portscan_result_t *result)
{
    if (!result)
        return;
    free(result->ports);
    free(result);
}

void ph_portscan_print_result(const ph_portscan_result_t *result, int open_only)
{
    if (!result || !result->ports)
        return;

    printf("\n%-10s %-12s %s\n", "PORT", "STATE", "PROTOCOL");
    printf("%-10s %-12s %s\n", "----", "-----", "--------");

    for (size_t i = 0; i < result->scanned_count; i++) {
        const char *state;
        switch (result->ports[i].state) {
        case PH_PORT_OPEN:
            state = "open";
            break;
        case PH_PORT_CLOSED:
            state = "closed";
            break;
        case PH_PORT_FILTERED:
            state = "filtered";
            break;
        default:
            state = "unknown";
            break;
        }

        if (open_only && result->ports[i].state != PH_PORT_OPEN)
            continue;

        printf("%-10u %-12s tcp\n", result->ports[i].port, state);
    }

    printf("\nScanned %zu ports on %s in %.2fs -- %zu open\n", result->scanned_count,
           result->target, result->elapsed_ms / 1000.0, result->open_count);
}
