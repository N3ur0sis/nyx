# Changelog

All notable changes to NYX are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2025-06-01

### Added

- **Framework core**: CLI parsing, unified logging, thread-safe error handling with domain codes and suggestions, terminal UX (spinners, progress bars), privilege checking and auto-escalation
- **Network library**: interface management, CIDR parsing, packet crafting (ICMP, ARP, TCP SYN), raw socket abstraction
- **Output system**: zero-dependency JSON builder/parser, structured output envelopes, session-based persistence
- **Interactive shell**: shared REPL framework with linenoise line editing, persistent history, context-aware tab completion
- **Tool registry**: global in-process tool registry for workflow and shell invocation
- **Workflow engine**: DAG-based runtime with JSON workflow files, expression language with pipe operations (filter, select, count, first, flat), for_each fan-out, when conditionals, topological execution via Kahn's algorithm
- **pingsweep**: multi-threaded ICMP host discovery with per-host latency
- **portscan**: TCP port-state scanner with Connect and SYN (half-open) modes, top-ports support
- **macspoof**: MAC address spoofing with random generation, custom set, and restore
- **arpspoof**: ARP cache poisoning with bidirectional mode and automatic ARP restoration on exit
- **Master CLI** (`nyx`): interactive shell with tool contexts, workflow execution, session management
- **Workflow runner** (`nyx-run`): standalone workflow execution with live output streaming
- **Documentation**: workflow authoring guide, tool reference, contributor guide, library API docs
- **MkDocs site**: Material theme with full navigation, published to GitHub Pages
- **Security hardening**: all binaries built with stack protectors, FORTIFY_SOURCE, PIE, full RELRO, CF protection
- **CI/CD**: GitHub Actions for build, format check, docs validation, release packaging, and Pages deployment

[Unreleased]: https://github.com/N3ur0sis/nyx/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/N3ur0sis/nyx/releases/tag/v1.0.0
