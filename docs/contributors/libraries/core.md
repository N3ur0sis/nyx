# libs/core -- Core Framework Library

**Header directory:** `libs/core/include/core/`

The core library provides foundational utilities used by every tool and library in the framework.

## nyx_cli.h -- CLI Parsing

Standardized command-line argument parsing with help formatting.

### Key Types

- `nyx_cli_opt_def_t` -- option definition (short char, long name, argument type, flags)
- `nyx_cli_result_t` -- parsed result containing option values and extra arguments
- `nyx_cli_banner_config_t` -- banner display configuration

### Key Functions

| Function | Purpose |
|----------|---------|
| `nyx_cli_parse(argc, argv, defs, count)` | Parse command-line arguments against definitions |
| `nyx_cli_free_result(result)` | Free a parsed result |
| `nyx_cli_has_option(result, short_opt)` | Check if a flag was provided |
| `nyx_cli_get_option(result, short_opt)` | Get the value of a flag |
| `nyx_cli_print_usage(name, defs, count, desc)` | Print formatted help text |
| `nyx_cli_validate_required(result, defs, count)` | Check all required options are present |
| `nyx_cli_print_banner(config)` | Display a styled ASCII art banner |
| `nyx_cli_validate_ipv4(ip)` | Validate an IPv4 address string |
| `nyx_cli_validate_mac(mac)` | Validate a MAC address string |
| `nyx_cli_validate_cidr(cidr)` | Validate a CIDR notation string |

### Usage Pattern

```c
static const nyx_cli_opt_def_t options[] = {
    {'c', "cidr", "CIDR", "Target subnet", NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_REQUIRED},
    {'T', "threads", "N", "Thread count", NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_OPTIONAL},
    {'h', "help", NULL, "Show help", NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
};
nyx_cli_result_t *r = nyx_cli_parse(argc, argv, options, 3);
const char *cidr = nyx_cli_get_option(r, 'c');
nyx_cli_free_result(r);
```

## nyx_error.h -- Error Handling

Thread-safe error context with domain codes, severity levels, and actionable suggestions.

### Key Types

- `nyx_error_context_t` -- thread-local error state (code, domain, severity, message, suggestion)
- `nyx_error_severity_t` -- `INFO`, `WARNING`, `ERROR`, `CRITICAL`

### Error Domains

| Constant | Value | Module |
|----------|-------|--------|
| `NYX_DOMAIN_CORE` | 0 | Core framework |
| `NYX_DOMAIN_IFACE` | 1 | Network interfaces |
| `NYX_DOMAIN_MACSPOOF` | 2 | MAC spoofing |
| `NYX_DOMAIN_ARPSPOOF` | 3 | ARP spoofing |
| `NYX_DOMAIN_PINGSWEEP` | 4 | Ping sweep |
| `NYX_DOMAIN_PORTSCAN` | 5 | Port scanning |
| `NYX_DOMAIN_NETADDR` | 6 | Network addressing |

### Key Macros

```c
NYX_ERROR_SET(domain, code, "format %s", arg);
NYX_ERROR_SET_EX(domain, code, severity, "message", "suggestion");
```

Both macros automatically capture `__FILE__`, `__LINE__`, and `__func__`.

### Key Functions

| Function | Purpose |
|----------|---------|
| `nyx_error_get()` | Get the current thread's error context |
| `nyx_error_clear()` | Clear the error context |
| `nyx_error_str(domain, code)` | Human-readable string for an error code |
| `nyx_error_log(level, show_details)` | Log the current error via `nyx_logger` |
| `nyx_error_to_core(domain, code)` | Translate a domain-specific code to a core code |

## nyx_logger.h -- Logging

Color-coded console logging with verbosity control.

### Log Levels

| Level | Color | Usage |
|-------|-------|-------|
| `NYX_LOG_INFO` | Cyan | General information |
| `NYX_LOG_WARN` | Yellow | Warnings |
| `NYX_LOG_ERROR` | Red | Errors |
| `NYX_LOG_SUCCESS` | Green | Successful operations |
| `NYX_LOG_VERBOSE` | Magenta | Debug output (only when verbose is enabled) |

### Functions

```c
nyx_log(NYX_LOG_INFO, "Scanning %s...", cidr);
nyx_set_verbose(1);  /* enable NYX_LOG_VERBOSE output */
```

## nyx_term.h -- Terminal UX

Status lines, progress bars, and spinners for interactive tools.

| Function | Purpose |
|----------|---------|
| `nyx_term_set_enabled(1)` | Enable terminal UX (auto-disabled for non-TTY) |
| `nyx_term_is_interactive()` | Check if stdout is a TTY |
| `nyx_term_statusf(fmt, ...)` | Display a status line (overwritten by next call) |
| `nyx_term_progress(label, current, total)` | Show a progress bar |
| `nyx_term_spinner(label, tick)` | Show a rotating spinner |
| `nyx_term_clear_status()` | Clear the status line |
| `nyx_term_suspend()` / `nyx_term_resume()` | Pause/resume status output |

## nyx_priv.h -- Privilege Management

Check and escalate process privileges for tools that need raw sockets or interface control.

### Privilege Flags

| Flag | Meaning |
|------|---------|
| `NYX_PRIV_NONE` | No special privileges |
| `NYX_PRIV_NET_RAW` | Raw/packet sockets |
| `NYX_PRIV_NET_ADMIN` | Interface configuration |

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_priv_check(priv)` | Check if the process has the requested privileges |
| `nyx_priv_escalate(argc, argv)` | Re-exec with `sudo` (TTY only, never returns on success) |
| `nyx_priv_ensure(priv, argc, argv)` | Check + escalate if needed |
| `nyx_priv_label(priv)` | Human-readable privilege description |

### Usage Pattern

```c
if (nyx_priv_ensure(NYX_PRIV_NET_RAW, argc, argv) != 0) {
    fprintf(stderr, "Requires root.\n");
    return 1;
}
```
