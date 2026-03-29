# pingsweep

Multi-threaded ICMP ping sweep for network host discovery.

**Module:** phobos
**Privileges:** root (`CAP_NET_RAW`)

## What It Does

Sends ICMP Echo Request packets to every address in a CIDR range and reports which hosts respond, along with their round-trip latency.

## Interactive Commands

### scan

```
scan -c <cidr> [-i iface] [-t ms] [-T N]
```

| Flag | Long | Description | Default |
|------|------|-------------|---------|
| `-c` | `--cidr` | Target CIDR range (required) | -- |
| `-i` | `--interface` | Network interface to use | auto-detected |
| `-t` | `--timeout` | Per-host timeout in milliseconds | 1000 |
| `-T` | `--threads` | Number of concurrent threads | 32 |

Examples:

```
pingsweep> scan -c 192.168.1.0/24
pingsweep> scan -c 10.0.0.0/8 -T 64 -t 500
```

### list

```
list
```

Shows all network interfaces with their IP and MAC addresses.

## Workflow Contract

### Accepted Parameters

| Key | Aliases | Type | Required | Description |
|-----|---------|------|----------|-------------|
| `c` | `cidr` | string | yes | Target CIDR range |
| `i` | `interface` | string | no | Network interface |
| `t` | `timeout` | integer | no | Timeout in ms (default: 1000) |
| `T` | `threads` | integer | no | Thread count (default: 32) |

### Result Fields

```json
{
  "results": {
    "hosts": [
      { "ip": "192.168.1.1", "alive": true, "latency_ms": 1.23 },
      { "ip": "192.168.1.2", "alive": false, "latency_ms": 0.0 }
    ],
    "total": 254,
    "alive_count": 2,
    "elapsed_ms": 3200.5
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `hosts` | array | One entry per address in the range |
| `hosts[].ip` | string | IPv4 address |
| `hosts[].alive` | bool | Whether the host responded |
| `hosts[].latency_ms` | float | Round-trip time (0 if not alive) |
| `total` | int | Total addresses scanned |
| `alive_count` | int | Number of hosts that responded |
| `elapsed_ms` | float | Total scan duration |

### Workflow Example

```json
{
  "id": "sweep",
  "tool": "pingsweep",
  "params": {
    "c": "${vars.subnet}",
    "T": 8,
    "t": 1000
  }
}
```

### Common Wiring Patterns

Filter alive hosts for downstream processing:

```
${sweep.results.hosts | filter alive == true}
```

Extract just the IPs of alive hosts:

```
${sweep.results.hosts | filter alive == true | select ip}
```

Guard a downstream step:

```json
"when": "${sweep.results.alive_count > 0}"
```

### Error Conditions

| Error | Cause | Suggestion |
|-------|-------|------------|
| No CIDR target specified | Missing `c` / `cidr` parameter | Add `"c": "10.0.0.0/24"` to params |
| Requires root privileges | Not running as root | Run with `sudo` or set `CAP_NET_RAW` |
| Invalid CIDR | Malformed CIDR string | Check format: `A.B.C.D/prefix` |
