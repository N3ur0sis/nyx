# Example: Network Discovery

This walkthrough explains `workflows/net-discovery.json`, the reference workflow that ships with NYX. It sweeps a subnet for alive hosts, then port-scans each one.

## The Workflow File

```json
{
  "workflow": {
    "id": "net-discovery",
    "name": "Network Discovery",
    "version": "1.0",
    "description": "Sweep a subnet for alive hosts, then port-scan each one"
  },
  "vars": {
    "subnet": "192.168.1.0/24",
    "top_ports": 100,
    "scan_threads": 8,
    "mode": "connect"
  },
  "steps": [
    {
      "id": "sweep",
      "tool": "nyx-pingsweep",
      "params": {
        "c": "${vars.subnet}",
        "T": "${vars.scan_threads}",
        "t": 1000
      }
    },
    {
      "id": "portscan",
      "tool": "nyx-portscan",
      "for_each": "${sweep.results.hosts | filter alive == true}",
      "when": "${sweep.results.alive_count > 0}",
      "params": {
        "t": "${each.ip}",
        "P": "${vars.top_ports}",
        "m": "${vars.mode}",
        "w": 2000
      }
    }
  ]
}
```

## Step-by-Step Breakdown

### Variables

Four variables are declared with defaults:

| Variable | Default | Purpose |
|----------|---------|---------|
| `subnet` | `192.168.1.0/24` | CIDR range to sweep |
| `top_ports` | `100` | Number of top ports to scan per host |
| `scan_threads` | `8` | Thread count for the sweep |
| `mode` | `connect` | Port scan mode (connect or syn) |

Override from the command line:

```bash
nyx run workflows/net-discovery.json --var subnet=10.0.0.0/24 --var mode=syn
```

### Step 1: sweep

Runs `pingsweep` on the target subnet.

- `c` receives the subnet variable
- `T` receives the thread count
- `t` is a literal integer (1000 ms timeout)

The pingsweep tool produces results like:

```json
{
  "hosts": [
    { "ip": "192.168.1.1", "alive": true, "latency_ms": 1.23 },
    { "ip": "192.168.1.2", "alive": false, "latency_ms": 0.0 },
    { "ip": "192.168.1.5", "alive": true, "latency_ms": 4.56 }
  ],
  "total": 254,
  "alive_count": 2,
  "elapsed_ms": 3200.5
}
```

### Step 2: portscan

This step demonstrates both `for_each` and `when`:

**when**: `${sweep.results.alive_count > 0}` -- skip the entire step if nothing was found alive. This avoids iterating over an empty array.

**for_each**: `${sweep.results.hosts | filter alive == true}` -- filters the host array to only alive entries, then runs the step once per element.

Within each iteration:

- `${each.ip}` resolves to the current host's IP (e.g. `"192.168.1.1"`)
- `${vars.top_ports}` and `${vars.mode}` pull from workflow variables

Each iteration produces a portscan result envelope. The step's overall result is an array of these envelopes.

### Dependency Graph

```
vars
  └── sweep (pingsweep)
        └── portscan (portscan, for_each)
```

The engine detects that `portscan` references `sweep.results` and executes them in order. No explicit dependency declaration is needed.

## Running It

```bash
# Interactive
sudo nyx
nyx> run workflows/net-discovery.json

# Command line with override
sudo nyx run workflows/net-discovery.json --var subnet=10.0.0.0/24

# JSON output
sudo nyx-run -J workflows/net-discovery.json
```

In human mode, the engine streams each tool's output live with step context prefixes. In JSON mode, a single structured result envelope is emitted at the end.
