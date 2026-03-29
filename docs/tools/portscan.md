# portscan

TCP port-state scanner supporting Connect and SYN (half-open) modes.

**Module:** phobos
**Privileges:** none for Connect mode; root (`CAP_NET_RAW`) for SYN mode

## What It Does

Determines whether TCP ports on a target host are open, closed, or filtered. This is an atomic scanner -- it does not identify services, grab banners, or fingerprint versions.

Two scan modes:

- **TCP Connect** -- uses `connect(2)` with non-blocking sockets. No root needed.
- **TCP SYN** -- sends raw SYN packets and analyzes responses. Faster and stealthier. Requires root.

When SYN mode is requested but root is unavailable, the tool automatically falls back to Connect with a warning.

## Interactive Commands

### scan

```
scan -t <ip> [options]
```

| Flag | Long | Description | Default |
|------|------|-------------|---------|
| `-t` | `--target` | Target IPv4 address (required) | -- |
| `-p` | `--ports` | Port range (e.g. `1-1024`, `80`) | -- |
| `-P` | `--top-ports` | Scan top N common ports | 100 |
| `-m` | `--mode` | `connect` or `syn` | auto |
| `-T` | `--threads` | Concurrent scan threads | 16 |
| `-w` | `--timeout` | Per-port timeout in ms | 2000 |
| `-o` | `--open-only` | Only show open ports | off |

`-p` and `-P` are mutually exclusive. If neither is given, `-P 100` is used.

Examples:

```
portscan> scan -t 192.168.1.1
portscan> scan -t 10.0.0.5 -p 1-65535 -m syn -T 64
portscan> scan -t 10.0.0.5 -P 20 -o
```

## Workflow Contract

### Accepted Parameters

| Key | Aliases | Type | Required | Description |
|-----|---------|------|----------|-------------|
| `t` | `target` | string | yes | Target IPv4 address |
| `p` | `ports` | string | no | Port range (`start-end` or single port) |
| `P` | `top-ports` | integer | no | Scan top N ports (default: 100) |
| `m` | `mode` | string | no | `connect` or `syn` (default: auto) |
| `T` | `threads` | integer | no | Thread count (default: 16) |
| `w` | `timeout` | integer | no | Timeout in ms (default: 2000) |
| `o` | `open-only` | bool | no | Filter to open ports only |

### Result Fields

```json
{
  "results": {
    "target": "192.168.1.1",
    "ports": [
      { "port": 22, "state": "open", "protocol": "tcp" },
      { "port": 80, "state": "open", "protocol": "tcp" },
      { "port": 443, "state": "closed", "protocol": "tcp" }
    ],
    "open_count": 2,
    "scanned_count": 100,
    "elapsed_ms": 5432.1,
    "scan_mode": "connect"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `target` | string | Target IP that was scanned |
| `ports` | array | Port results (filtered if `open-only`) |
| `ports[].port` | int | Port number |
| `ports[].state` | string | `open`, `closed`, or `filtered` |
| `ports[].protocol` | string | Always `tcp` |
| `open_count` | int | Number of open ports found |
| `scanned_count` | int | Total ports scanned |
| `elapsed_ms` | float | Scan duration |
| `scan_mode` | string | Actual mode used (`connect` or `syn`) |

### Workflow Example

```json
{
  "id": "scan",
  "tool": "portscan",
  "params": {
    "t": "${each.ip}",
    "P": "${vars.top_ports}",
    "m": "${vars.mode}",
    "w": 2000
  }
}
```

### Common Wiring Patterns

Scan all alive hosts from a sweep, using `for_each`:

```json
{
  "id": "scan_all",
  "tool": "portscan",
  "for_each": "${sweep.results.hosts | filter alive == true}",
  "when": "${sweep.results.alive_count > 0}",
  "params": {
    "t": "${each.ip}",
    "P": 100
  }
}
```

### Error Conditions

| Error | Cause | Suggestion |
|-------|-------|------------|
| No target IP specified | Missing `t` / `target` parameter | Add `"t": "10.0.0.1"` to params |
| Invalid port range | Malformed port string | Use `start-end` (e.g. `1-1024`) or single number |
| SYN mode fallback | Requested `syn` without root | Run with `sudo` or use `connect` mode |
