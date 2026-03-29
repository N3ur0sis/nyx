# Tool Contracts

A tool's contract is the interface between the tool and the rest of the framework: the workflow engine, the interactive shell, and JSON consumers. The contract is defined entirely in the command layer (`*_cmd.c`).

## The Invoke Function

Every tool's entry point for the framework is a function with this signature:

```c
int my_invoke(const nyx_json_t *params, nyx_output_ctx_t *out);
```

**params**: a JSON object containing the tool's input parameters. Keys are either short flags (e.g. `"c"`) or long names (e.g. `"cidr"`). The function should accept both.

**out**: a pre-initialized output context. The function must populate:

1. **config** -- echo the resolved configuration back (for debugging and reproducibility)
2. **results** -- the tool's output data
3. **status** -- `"success"` or `"error"`
4. **error** -- set from `nyx_error` context on failure

The function must **not** call `nyx_output_finish()`. The caller decides when and how to flush the envelope.

## Parameter Conventions

- Accept **both** short and long key names: `"c"` and `"cidr"`, `"t"` and `"target"`
- Parse string values that look like numbers with `strtol` to handle workflows passing strings
- Use sensible defaults when parameters are missing
- Set a detailed error (with suggestion) and return immediately if a required parameter is missing

## Output Envelope

The tool's output is wrapped in a standard envelope:

```json
{
  "nyx": {
    "tool": "pingsweep",
    "module": "phobos",
    "version": "1.0",
    "timestamp": "2025-06-15T14:30:00Z",
    "duration_ms": 3200.5
  },
  "config": { /* echoed config */ },
  "status": "success",
  "error": null,
  "results": { /* tool-specific data */ }
}
```

Workflows and downstream tools access `results` through expressions like `${sweep.results.hosts}`.

## Result Design Guidelines

- Use flat, predictable key names (`alive_count`, not `stats.alive.total`)
- Emit arrays of objects for collections (`hosts[]`, `ports[]`)
- Include count fields alongside arrays (`alive_count`, `open_count`)
- Include timing (`elapsed_ms`) for performance analysis
- Use consistent types: strings for IPs and MACs, booleans for state, numbers for counts

## Error Handling

On failure:

```c
NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_CIDR,
                 NYX_ERROR_SEV_ERROR,
                 "Invalid CIDR notation",
                 "Use format: A.B.C.D/prefix (e.g. 192.168.1.0/24)");
nyx_output_set_error_from_ctx(out);
return PH_PINGSWEEP_ERR_CIDR;
```

This produces an error envelope:

```json
{
  "status": "error",
  "error": {
    "code": -2,
    "domain": "pingsweep",
    "severity": "error",
    "message": "Invalid CIDR notation",
    "suggestion": "Use format: A.B.C.D/prefix (e.g. 192.168.1.0/24)"
  },
  "results": null
}
```

Every error must include:

- A specific error code (not just -1)
- A human-readable message explaining what went wrong
- A suggestion for how to fix it

## REPL Command Definitions

The command layer also defines the REPL commands:

```c
const nyx_repl_cmd_t my_repl_cmds[] = {
    {
        .name = "scan",
        .usage = "scan -c <cidr> [-t ms]",
        .description = "Short description for help summary",
        .help = "  Detailed multi-line help...\n",
        .handler = repl_scan,
        .flags = scan_flags,
        .flag_count = sizeof(scan_flags) / sizeof(scan_flags[0])
    },
};
```

- **name**: what the user types
- **usage**: one-line synopsis shown in help
- **description**: short description for the help table
- **help**: multi-line detailed help shown by `help <cmd>`
- **flags**: array of `nyx_repl_flag_t` for tab completion
- **handler**: function called when the command is invoked

## Registration

```c
void my_tool_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name          = "mytool",
        .module        = "phobos",
        .version       = "1.0",
        .description   = "One-line tool description",
        .invoke        = my_invoke,
        .cmds          = my_repl_cmds,
        .cmd_count     = MY_REPL_CMD_COUNT,
        .required_priv = NYX_PRIV_NONE
    });
}
```

Set `required_priv` to the correct bitmask:

| Value | Meaning |
|-------|---------|
| `NYX_PRIV_NONE` | No special privileges needed |
| `NYX_PRIV_NET_RAW` | Raw sockets (pingsweep, portscan SYN, arpspoof) |
| `NYX_PRIV_NET_ADMIN` | Interface configuration (macspoof) |
| `NYX_PRIV_NET_RAW \| NYX_PRIV_NET_ADMIN` | Both |
