# Output Envelope Reference

Every NYX tool run produces a JSON envelope with this structure:

```json
{
  "nyx": {
    "tool": "pingsweep",
    "module": "phobos",
    "version": "1.0",
    "timestamp": "2025-06-15T14:30:00Z",
    "duration_ms": 3200.5
  },
  "config": {
    "cidr": "192.168.1.0/24",
    "interface": "eth0",
    "timeout_ms": 1000,
    "threads": 8
  },
  "status": "success",
  "error": null,
  "results": {
    "hosts": [ ... ],
    "total": 254,
    "alive_count": 12,
    "elapsed_ms": 3200.5
  }
}
```

## Sections

### nyx

Tool metadata, populated automatically by `nyx_output_init()`:

| Field | Type | Description |
|-------|------|-------------|
| `tool` | string | Tool name |
| `module` | string | Module name (e.g. `"phobos"`) |
| `version` | string | Tool version |
| `timestamp` | string | ISO 8601 UTC timestamp of when the run started |
| `duration_ms` | float | Total execution time in milliseconds |

### config

Echo of the resolved configuration used for this run. Set by the tool's command layer via `nyx_output_set_config()`. Useful for reproducibility and debugging.

### status

One of:

| Value | Meaning |
|-------|---------|
| `"success"` | Tool completed without errors |
| `"error"` | Tool encountered an error |
| `"partial"` | Some results succeeded, some failed |

### error

`null` on success. On error:

```json
{
  "error": {
    "code": -2,
    "domain": "pingsweep",
    "severity": "error",
    "message": "Invalid CIDR notation",
    "suggestion": "Use format: A.B.C.D/prefix (e.g. 192.168.1.0/24)"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `code` | int | Domain-specific error code |
| `domain` | string | Error domain name |
| `severity` | string | `"info"`, `"warning"`, `"error"`, or `"critical"` |
| `message` | string | Human-readable error description |
| `suggestion` | string | Actionable fix suggestion |

### results

Tool-specific output data. See each tool's documentation for the exact fields:

- [pingsweep](../tools/pingsweep.md#result-fields)
- [portscan](../tools/portscan.md#result-fields)
- [macspoof](../tools/macspoof.md#result-fields)
- [arpspoof](../tools/arpspoof.md#result-fields)

## Accessing Envelopes

### JSON Mode

Add `-J` to any tool command to write the envelope to stdout:

```bash
nyx-portscan -t 192.168.1.1 -P 20 -J
```

### Session Mode

Add `-S <id>` to write the envelope to a session file:

```bash
nyx-pingsweep -c 192.168.1.0/24 -J -S mysession
# Written to ~/.nyx/sessions/mysession/pingsweep.nyx.json
```

### Workflow Mode

The workflow engine captures envelopes from each step automatically. Access them via `nyx_wf_get_results()` or `nyx session show <id>`.

### Programmatic Loading

```c
nyx_json_t *data = nyx_output_load_results("mysession", "pingsweep");
const nyx_json_t *hosts = nyx_json_get(
    nyx_json_get(data, "results"), "hosts");
nyx_json_free(data);
```

## In Workflows

Expressions navigate into the `results` section of a step's envelope:

```
${sweep.results.hosts}           -- the hosts array
${sweep.results.alive_count}     -- integer
${scan.results.ports | filter state == open}
```

The `config`, `nyx`, `status`, and `error` fields are also accessible:

```
${sweep.status}                  -- "success" or "error"
```
