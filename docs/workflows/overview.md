# Workflow Authoring Guide

NYX workflows are JSON files that describe a directed acyclic graph (DAG) of tool invocations. The workflow engine resolves dependencies between steps automatically, executes them in topological order, and wires results from one step into the parameters of the next using an expression language.

## Why Workflows

Penetration testing follows repeatable patterns: discover hosts, scan ports, identify services, attempt exploits. NYX workflows encode these patterns as declarative configuration files that anyone can read, share, and modify without writing code.

## Concepts

- **Step** -- a single tool invocation with parameters
- **Expression** -- a `${...}` reference that pulls data from previous step results or workflow variables
- **for_each** -- fan-out: run a step once per item in an array
- **when** -- conditional: skip a step if a boolean expression is false
- **Session** -- persistent storage for all tool outputs during a workflow run

## Running a Workflow

```bash
# From the nyx interactive shell
nyx> run workflows/net-discovery.json

# From the command line
nyx run workflows/net-discovery.json
nyx run workflows/net-discovery.json --var subnet=10.0.0.0/24

# Standalone runner
nyx-run workflows/net-discovery.json -V subnet=10.0.0.0/24
```

Add `-J` for JSON output or `-S <id>` to write results to a named session.

## Next Steps

- [File Format](file-format.md) -- structure of a workflow JSON file
- [Expression Language](expressions.md) -- `${...}` syntax and pipe operations
- [for_each and when](for-each-and-when.md) -- iteration and conditional execution
- [Example: Network Discovery](examples/net-discovery.md) -- worked walkthrough
