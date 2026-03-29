# Tool Reference

Every NYX tool is an atomic, single-purpose binary with a consistent interface:

- **Interactive shell** -- the default mode. Launches a REPL with tab completion, history, and contextual help.
- **JSON mode** (`-J`) -- emits a structured output envelope to stdout for scripting and workflow consumption.
- **Session mode** (`-S <id>`) -- writes results to `~/.nyx/sessions/<id>/` for later retrieval.
- **Workflow mode** -- the workflow engine invokes the tool in-process through the tool registry.

## Shipped Tools

| Tool | Module | Description | Privileges |
|------|--------|-------------|------------|
| [pingsweep](pingsweep.md) | phobos | Multi-threaded ICMP host discovery | `CAP_NET_RAW` (root) |
| [portscan](portscan.md) | phobos | TCP port-state scanner (Connect / SYN) | none; SYN mode needs root |
| [macspoof](macspoof.md) | phobos | MAC address manipulation | `CAP_NET_ADMIN` (root) |
| [arpspoof](arpspoof.md) | phobos | ARP cache poisoning for MITM | `CAP_NET_RAW` (root) |

## Interactive Usage

Every tool can be launched standalone or from within the `nyx` master shell:

```bash
# Standalone
sudo ./bin/nyx-pingsweep
pingsweep> scan -c 192.168.1.0/24
pingsweep> help scan
pingsweep> exit

# From the master shell
sudo ./bin/nyx
nyx> pingsweep
nyx:pingsweep> scan -c 192.168.1.0/24
nyx:pingsweep> back
nyx>
```

Built-in shell commands available in every tool:

| Command | Description |
|---------|-------------|
| `help` | List all commands |
| `help <cmd>` | Show detailed help for a command |
| `history` | Show command history |
| `clear` | Clear the terminal |
| `exit` / `quit` | Exit the shell |
| `back` | Return to parent context (master shell only) |

## Privilege Handling

Tools that require root privileges will attempt automatic `sudo` re-execution when launched from a TTY. If you run `./bin/nyx-pingsweep` without root, it will prompt for your `sudo` password and restart itself.

From the `nyx` master shell, the process cannot re-exec mid-session. Instead, a warning is displayed suggesting you restart with `sudo nyx`.

The workflow engine performs a pre-flight privilege check before execution and warns if any step requires privileges not currently held.
