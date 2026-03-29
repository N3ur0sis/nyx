# NYX

```
███╗   ██╗██╗   ██╗██╗  ██╗
████╗  ██║╚██╗ ██╔╝╚██╗██╔╝
██╔██╗ ██║ ╚████╔╝  ╚███╔╝
██║╚██╗██║  ╚██╔╝   ██╔██╗
██║ ╚████║   ██║   ██╔╝ ██╗
╚═╝  ╚═══╝   ╚═╝   ╚═╝  ╚═╝
```

**A modular penetration testing framework built in C**

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)]()

---

NYX is a suite of atomic, single-purpose security tools connected by a structured output system and a DAG-based workflow engine. Tools communicate through JSON envelopes so their results can be chained in automated workflows or consumed by scripts, GUIs, and LLMs.

The framework runs on Linux and targets penetration testers, security researchers, and red team operators who want repeatable, composable toolchains.

## Current Tools

All tools ship as interactive shells with tab completion, history, and detailed help. They also accept `-J` for machine-readable JSON output and integrate with the workflow engine.

| Tool | Module | Description | Privileges |
|------|--------|-------------|------------|
| **pingsweep** | phobos | Multi-threaded ICMP host discovery | root |
| **portscan** | phobos | TCP port-state scanner (Connect / SYN) | none (SYN needs root) |
| **macspoof** | phobos | MAC address spoofing, randomization, restore | root |
| **arpspoof** | phobos | ARP cache poisoning for MITM | root |

## Quick Start

```bash
git clone https://github.com/N3ur0sis/nyx.git
cd nyx
cmake -B build
cmake --build build
```

Launch the master interactive shell:

```bash
sudo ./bin/nyx
```

Or run a tool directly:

```bash
sudo ./bin/nyx-pingsweep    # enters the pingsweep shell
./bin/nyx-portscan           # enters the portscan shell (connect mode)
```

Run a workflow:

```bash
sudo ./bin/nyx run workflows/net-discovery.json
sudo ./bin/nyx run workflows/net-discovery.json --var subnet=10.0.0.0/24
```

## Architecture

```
nyx/
├── libs/
│   ├── core/        # CLI, logging, errors, terminal UX, privileges
│   ├── network/     # Interfaces, CIDR parsing, packet crafting, sockets
│   ├── output/      # JSON builder/parser, structured output envelopes
│   ├── shell/       # Shared REPL (linenoise), tool registry
│   └── workflow/    # DAG parser, expression engine, execution runtime
├── tools/
│   ├── phobos/      # Network-layer tools (pingsweep, portscan, macspoof, arpspoof)
│   ├── nyx/         # Master interactive shell
│   └── nyx-run/     # Standalone workflow runner
└── workflows/       # Example workflow definitions
```

Every tool follows the same layered pattern:

- **API + Implementation** -- core logic, no I/O assumptions
- **Command layer** -- JSON parameter parsing, output envelope population, tool registry entry
- **Interactive frontend** -- REPL shell using the shared `nyx_repl` library

The workflow engine invokes tools in-process through the global registry, not by spawning child processes.

## Documentation

- **[Workflow Authoring Guide](docs/workflows/overview.md)** -- how to write NYX workflows
- **[Tool Reference](docs/tools/overview.md)** -- parameters, outputs, and examples for each tool
- **[Contributor Guide](docs/contributors/architecture.md)** -- architecture, shared libraries, adding tools
- **[Output Envelope Reference](docs/reference/output-envelope.md)** -- JSON structure for tool results

## Building

```bash
cmake -B build                          # configure
cmake --build build                     # build all tools
cmake --build build --target nyx-portscan  # build one tool
sudo cmake --install build              # install binaries and man pages
```

Debug build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

All binaries are hardened with `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`, PIE, full RELRO, and `-fcf-protection` where supported.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code style, and how to submit pull requests.

## Security

To report a vulnerability in NYX itself, see [SECURITY.md](SECURITY.md).

## Legal Disclaimer

NYX is designed for **authorized security testing, research, and educational purposes only**. Users are responsible for complying with all applicable laws in their jurisdiction. The authors assume no liability for misuse.

## License

MIT License. See [LICENSE](LICENSE) for details.

Copyright (c) 2025 Neur0sis
