# libs/network -- Network Library

**Header directory:** `libs/network/include/network/`

Reusable networking primitives used across all network-layer tools: interface management, address parsing, packet crafting, and raw sockets.

## nyx_iface.h -- Interface Utilities

Manage and query network interfaces.

### Status Codes

| Code | Meaning |
|------|---------|
| `NYX_IFACE_SUCCESS` (0) | Operation succeeded |
| `NYX_IFACE_ERR_NOTFOUND` (-3) | Interface not found |
| `NYX_IFACE_ERR_PERM` (-5) | Permission denied |
| `NYX_IFACE_ERR_SOCKET` (-6) | Socket creation failed |

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_iface_is_valid(name)` | Check if an interface exists |
| `nyx_iface_is_up(name)` | Check if an interface is up |
| `nyx_iface_get_mac(name, buf, len)` | Get MAC address as string |
| `nyx_iface_get_ipv4(name, buf, len)` | Get IPv4 address as string |
| `nyx_iface_get_netmask(name, buf, len)` | Get netmask as string |
| `nyx_iface_list(names, max, count)` | List all interface names |
| `nyx_iface_get_mac_by_ip(iface, ip, buf, len)` | ARP resolve an IP to MAC |
| `nyx_iface_get_index(name)` | Get interface index |
| `nyx_iface_print_details()` | Print all interfaces to stdout |
| `nyx_iface_get_default_gateway(buf, len)` | Get default gateway interface name |
| `nyx_iface_get_gateway_ip(buf, len)` | Get default gateway IP |
| `nyx_iface_set_status(name, up)` | Bring interface up or down |
| `nyx_iface_add_ipv4(name, ip, prefix)` | Add an IPv4 address to an interface |

## nyx_netaddr.h -- Address Utilities

CIDR parsing, subnet math, and address validation.

### Key Types

```c
typedef struct {
    char cidr_str[NYX_MAX_CIDR_LEN];
    uint32_t network;       /* Network address (host byte order) */
    uint32_t netmask;       /* Subnet mask */
    uint32_t first_host;    /* First usable host */
    uint32_t last_host;     /* Last usable host */
    uint32_t broadcast;     /* Broadcast address */
    uint8_t prefix_len;     /* CIDR prefix (e.g. 24) */
    uint32_t num_hosts;     /* Number of usable hosts */
} nyx_cidr_info_t;
```

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_netaddr_parse_cidr(str, info)` | Parse `"192.168.1.0/24"` into `nyx_cidr_info_t` |
| `nyx_netaddr_str_to_ip(str, ip)` | Parse `"192.168.1.1"` to `uint32_t` |
| `nyx_netaddr_ip_to_str(ip, buf, len)` | Format `uint32_t` as `"192.168.1.1"` |
| `nyx_netaddr_next_ip(current, last)` | Increment an IP (for iteration) |
| `nyx_netaddr_validate_ipv4(str)` | Validate IPv4 format |
| `nyx_netaddr_validate_mac(str)` | Validate MAC format |
| `nyx_netaddr_validate_cidr(str)` | Validate CIDR format |

### Usage Pattern

```c
nyx_cidr_info_t info;
if (nyx_netaddr_parse_cidr("10.0.0.0/24", &info) == NYX_NETADDR_SUCCESS) {
    printf("Hosts: %u\n", info.num_hosts);  /* 254 */
    uint32_t ip = info.first_host;
    while (ip <= info.last_host) {
        char buf[16];
        nyx_netaddr_ip_to_str(ip, buf, sizeof(buf));
        printf("%s\n", buf);
        nyx_netaddr_next_ip(&ip, info.last_host);
    }
}
```

## nyx_packet.h -- Packet Crafting

Create and parse ICMP, ARP, and TCP packets.

### Key Types

- `nyx_icmp_params_t` -- ICMP packet parameters (type, code, id, seq, data)
- `nyx_arp_params_t` -- ARP packet parameters (op, sender/target MAC and IP)
- `nyx_tcp_params_t` -- TCP parameters (src/dst IP and port, seq, flags, window)
- `nyx_tcp_parsed_t` -- Parsed TCP header fields

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_packet_checksum(data, len)` | Compute IP/ICMP checksum |
| `nyx_packet_create_icmp_echo(buf, len, id, seq, data, dlen)` | Build ICMP Echo Request |
| `nyx_packet_create_arp(buf, len, params)` | Build ARP packet |
| `nyx_packet_create_ip_tcp_syn(buf, len, params)` | Build IPv4+TCP SYN packet |
| `nyx_packet_tcp_checksum(src, dst, seg, len)` | TCP checksum with pseudo-header |
| `nyx_packet_parse_icmp(pkt, len, out)` | Parse ICMP from raw bytes |
| `nyx_packet_parse_arp(pkt, len, out)` | Parse ARP from raw bytes |
| `nyx_packet_parse_tcp(pkt, len, out)` | Parse TCP from raw IPv4 packet |

## nyx_socket.h -- Raw Sockets

Create and manage raw sockets for low-level network operations.

### Socket Types

| Constant | Description |
|----------|-------------|
| `NYX_SOCKET_RAW_ICMP` | AF_INET, SOCK_RAW, IPPROTO_ICMP |
| `NYX_SOCKET_RAW_IP` | AF_INET, SOCK_RAW, IPPROTO_RAW |
| `NYX_SOCKET_RAW_PACKET` | AF_PACKET, SOCK_RAW (layer 2) |
| `NYX_SOCKET_RAW_TCP` | AF_INET, SOCK_RAW, IPPROTO_TCP |

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_socket_create(type, fd)` | Create a raw socket |
| `nyx_socket_set_recv_timeout(fd, ms)` | Set receive timeout |
| `nyx_socket_set_send_timeout(fd, ms)` | Set send timeout |
| `nyx_socket_enable_broadcast(fd)` | Enable broadcast on socket |
| `nyx_socket_send(fd, pkt, len, dest, dlen)` | Send a packet |
| `nyx_socket_recv(fd, buf, len, src, slen)` | Receive a packet |
| `nyx_socket_close(fd)` | Close a socket |

### Usage Pattern

```c
int fd;
if (nyx_socket_create(NYX_SOCKET_RAW_ICMP, &fd) == NYX_SOCKET_SUCCESS) {
    nyx_socket_set_recv_timeout(fd, 1000);
    /* send/recv packets */
    nyx_socket_close(fd);
}
```
