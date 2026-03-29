# Interactive Shells

NYX is built around interactive shells. Every tool and the master `nyx` binary launch a persistent REPL (Read-Eval-Print Loop) with line editing, history, and tab completion.

## The Master Shell

```bash
sudo ./bin/nyx
```

```
nyx> help
nyx> pingsweep          # enter tool context
nyx:pingsweep> help
nyx:pingsweep> scan -c 192.168.1.0/24
nyx:pingsweep> back     # return to master
nyx> run workflows/net-discovery.json
nyx> exit
```

### Master Shell Commands

| Command | Description |
|---------|-------------|
| `run <workflow.json>` | Execute a workflow file |
| `session list` | List past sessions |
| `session show <id>` | Show session details |
| `session clean` | Remove old sessions |
| `info` | Show framework version and installed tools |
| `version` | Print version |
| *tool-name* | Enter that tool's interactive context |
| `help` | Show all commands |
| `exit` / `quit` | Exit the shell |

## Tool Shells

Each tool can be launched as a standalone shell:

```bash
sudo ./bin/nyx-pingsweep
pingsweep> scan -c 192.168.1.0/24
pingsweep> list
pingsweep> help scan
pingsweep> exit
```

Or from the master shell by typing the tool name:

```bash
nyx> portscan
nyx:portscan> scan -t 192.168.1.1 -P 20
nyx:portscan> back
```

## Line Editing

NYX uses [linenoise](https://github.com/antirez/linenoise) for line editing:

| Key | Action |
|-----|--------|
| Left/Right arrows | Move cursor within the line |
| Up/Down arrows | Navigate command history |
| Ctrl-A | Jump to start of line |
| Ctrl-E | Jump to end of line |
| Ctrl-W | Delete word backwards |
| Ctrl-U | Delete to start of line |
| Ctrl-K | Delete to end of line |
| Tab | Autocomplete |

## Tab Completion

Tab completes contextually:

- **Commands**: type part of a command name and press Tab
- **Flags**: after a command, Tab cycles through valid flags (`-c`, `--cidr`, etc.)
- **Interfaces**: after `-i` or `--interface`, Tab lists network interfaces
- **Paths**: after flags that accept file paths, Tab completes filesystem paths
- **Tool names**: in the master shell, Tab completes registered tool names

## History

Command history is persisted per shell in `~/.nyx/<name>_history` (e.g. `~/.nyx/nyx_history`, `~/.nyx/pingsweep_history`). History survives across sessions.

## Privilege Escalation

Tools that need root will attempt automatic escalation:

- **Standalone tool**: if launched without root from a TTY, the tool re-executes itself with `sudo` prepended
- **Master shell**: cannot re-exec mid-session. A warning is displayed suggesting `sudo nyx`
- **Workflows**: a pre-flight check warns about missing privileges before execution starts
