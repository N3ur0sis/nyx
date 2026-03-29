# Architecture

## Overview

NYX is structured as shared libraries plus tool modules, connected by a global tool registry. The workflow engine and interactive shells consume tools in-process rather than spawning child processes.

```
┌─────────────────────────────────────────────────────────┐
│                    Frontends                            │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐ │
│  │ nyx      │  │ nyx-run      │  │ nyx-<tool>        │ │
│  │ (master  │  │ (standalone  │  │ (standalone tool   │ │
│  │  shell)  │  │  wf runner)  │  │  shell)            │ │
│  └────┬─────┘  └──────┬───────┘  └──────┬────────────┘ │
│       │               │                 │               │
│  ┌────┴───────────────┴─────────────────┴─────────────┐ │
│  │              Tool Registry (nyx_shell)             │ │
│  │   pingsweep | portscan | macspoof | arpspoof       │ │
│  └────┬───────────────┬─────────────────┬─────────────┘ │
│       │               │                 │               │
│  ┌────┴────┐   ┌──────┴──────┐   ┌─────┴──────┐       │
│  │ Command │   │ Workflow    │   │ REPL       │       │
│  │ Layer   │   │ Engine      │   │ Framework  │       │
│  │ *_cmd.c │   │ nyx_wf_*   │   │ nyx_repl   │       │
│  └────┬────┘   └──────┬──────┘   └────────────┘       │
│       │               │                                 │
│  ┌────┴───────────────┴───────────────────────────────┐ │
│  │              Tool Implementation                   │ │
│  │   *_api.h  +  *_impl.c  (pure logic, no I/O)      │ │
│  └────┬───────────────────────────────────────────────┘ │
│       │                                                 │
│  ┌────┴───────────────────────────────────────────────┐ │
│  │              Shared Libraries                      │ │
│  │   nyx_core | nyx_network | nyx_output | nyx_shell  │ │
│  └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

## Libraries

| Library | Path | Purpose |
|---------|------|---------|
| `nyx_core` | `libs/core/` | CLI parsing, logging, error handling, terminal UX, privileges |
| `nyx_network` | `libs/network/` | Interface management, CIDR parsing, packet crafting, raw sockets |
| `nyx_output` | `libs/output/` | JSON builder/parser, structured output envelopes, session management |
| `nyx_shell` | `libs/shell/` | REPL framework (linenoise), tool registry |
| `nyx_workflow` | `libs/workflow/` | DAG parser, expression engine, workflow execution runtime |

All libraries are built as static libraries and linked into tool binaries.

## Tool Layering

Every tool is split into distinct layers:

### 1. API + Implementation (`*_api.h`, `*_impl.c`)

Pure tool logic with no I/O assumptions. Defines config structs, result structs, and functions like `ph_pingsweep_scan()`. This layer knows nothing about JSON, CLIs, or workflows.

### 2. Command Layer (`*_cmd.c`)

Bridges the gap between the tool implementation and the framework:

- Parses JSON parameters from the `nyx_json_t` object
- Calls the implementation with native config structs
- Builds the `nyx_output` envelope with results
- Defines REPL commands and registers the tool in the global registry

This is the **tool contract**: it defines which parameter keys a tool accepts and which result fields it emits.

### 3. Interactive Frontend (`*_cli.c`)

A thin `main()` that creates an `nyx_repl_t`, registers the tool's commands, handles privilege escalation, and enters the REPL loop. This produces the standalone `nyx-<tool>` binary.

## Build System

NYX uses CMake with a helper function `nyx_add_tool()` defined in `cmake/NyxHelpers.cmake`:

```cmake
nyx_add_tool(pingsweep
    LIB_SOURCES  src/ph_pingsweep_impl.c src/ph_pingsweep_cmd.c
    MAIN_SOURCE  src/ph_pingsweep_cli.c
)
```

This creates:

- `nyx_pingsweep` -- static library (impl + cmd layer), linked by the master shell and workflow engine
- `nyx-pingsweep` -- executable (cli frontend), standalone tool binary

The library links against `nyx_core`, `nyx_network`, `nyx_output`, and `nyx_shell` by default.

## Tool Registry

All tools register themselves in the global registry at startup via `nyx_tools_register_all()`, defined in `tools/nyx/src/nyx_tools_builtin.c`:

```c
void nyx_tools_register_all(void)
{
    ph_pingsweep_register();
    ph_portscan_register();
    ph_macspoof_register();
    ph_arpspoof_register();
}
```

Each tool's `*_register()` function calls `nyx_tool_registry_add()` with:

- Tool name, module, version, description
- The `invoke` function (command layer entry point)
- REPL command table
- Privilege requirements

The workflow engine and master shell both call `nyx_tool_registry_find()` to invoke tools in-process.

## Data Flow

### Interactive Mode

```
User input → REPL tokenizer → Command handler →
    JSON params → cmd_invoke() → Implementation → Output envelope →
    Human-readable display (nyx_logger)
```

### Workflow Mode

```
Workflow JSON → Parser → DAG builder → Topological sort →
    For each step: expression resolution → cmd_invoke() →
    Output envelope → Next step's expressions
```

### JSON Mode (`-J`)

```
CLI args → nyx_output_from_cli() → cmd_invoke() →
    nyx_output_finish() → JSON envelope to stdout
```

## Sessions

Sessions are directories under `~/.nyx/sessions/<uuid>/`. Each tool run within a session writes a `<tool>.nyx.json` file containing the full output envelope. Sessions are created automatically during workflow runs and can be created manually with `-S`.
