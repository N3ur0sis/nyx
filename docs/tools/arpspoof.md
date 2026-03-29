# arpspoof

ARP cache poisoning for traffic interception (man-in-the-middle).

**Module:** phobos
**Privileges:** root (`CAP_NET_RAW`)

## What It Does

Sends forged ARP reply packets to a target host, causing it to associate the attacker's MAC address with another IP (typically the default gateway). This redirects traffic through the attacker's machine for interception or analysis.

Supports bidirectional poisoning (both the target and the gateway) and automatically restores original ARP mappings on exit (Ctrl-C / SIGINT / SIGTERM).

## Interactive Commands

### start

```
start -i <iface> -t <target> -s <spoof> [-b] [-n sec]
```

| Flag | Long | Description | Default |
|------|------|-------------|---------|
| `-i` | `--interface` | Network interface (required) | -- |
| `-t` | `--target` | Victim IP address (required) | -- |
| `-s` | `--spoof` | IP to impersonate, e.g. the gateway (required) | -- |
| `-b` | `--bidirectional` | Poison both target and spoof host | off |
| `-n` | `--interval` | Seconds between ARP packets | 1 |

Press Ctrl-C to stop. ARP tables are restored automatically.

Examples:

```
arpspoof> start -i eth0 -t 192.168.1.50 -s 192.168.1.1
arpspoof> start -i wlan0 -t 10.0.0.5 -s 10.0.0.1 -b -n 2
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
| `i` | `interface` | string | yes | Network interface |
| `t` | `target` | string | yes | Target (victim) IP address |
| `s` | `spoof` | string | yes | IP address to impersonate |
| `b` | `bidirectional` | bool | no | Poison both directions (default: false) |
| `n` | `interval` | integer | no | Interval between packets in seconds (default: 1) |

### Result Fields

```json
{
  "results": {
    "duration_s": 45.2,
    "restored": true
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `duration_s` | float | How long the poisoning ran |
| `restored` | bool | Whether ARP tables were restored on exit |

### Workflow Example

```json
{
  "id": "poison",
  "tool": "arpspoof",
  "params": {
    "i": "${vars.interface}",
    "t": "${each.ip}",
    "s": "${vars.gateway}",
    "b": true
  }
}
```

Note: arpspoof is a long-running tool (it runs until interrupted). In workflows, consider whether blocking execution is appropriate for your use case.

### Error Conditions

| Error | Cause | Suggestion |
|-------|-------|------------|
| Missing required parameters | Missing `i`, `t`, or `s` | Provide all three: interface, target, spoof |
| Requires root privileges | Not running as root | Run with `sudo` or set `CAP_NET_RAW` |
| ARP resolution failure | Cannot resolve target or spoof MAC | Verify both IPs are reachable on the local segment |
