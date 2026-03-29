# NYX Documentation

NYX is a modular penetration testing framework. Tools are atomic, single-purpose binaries that produce structured JSON output and can be chained together through a DAG-based workflow engine.

## For Workflow Authors

Write automated attack chains by wiring tools together in JSON workflow files.

- [Workflow Overview](workflows/overview.md)
- [File Format](workflows/file-format.md)
- [Expression Language](workflows/expressions.md)
- [for_each and when](workflows/for-each-and-when.md)
- [Example: Network Discovery](workflows/examples/net-discovery.md)

## Tool Reference

Each tool documents its accepted parameters, output fields, and privilege requirements.

- [Tool Overview](tools/overview.md)
- [pingsweep](tools/pingsweep.md) -- ICMP host discovery
- [portscan](tools/portscan.md) -- TCP port scanning
- [macspoof](tools/macspoof.md) -- MAC address spoofing
- [arpspoof](tools/arpspoof.md) -- ARP cache poisoning

## For Contributors

Build new tools, extend shared libraries, and understand the architecture.

- [Architecture](contributors/architecture.md)
- [Adding a New Tool](contributors/add-a-tool.md)
- [Tool Contracts](contributors/tool-contracts.md)
- Shared Libraries: [core](contributors/libraries/core.md) | [network](contributors/libraries/network.md) | [output](contributors/libraries/output.md) | [shell](contributors/libraries/shell.md) | [workflow](contributors/libraries/workflow.md)

## Reference

- [Output Envelope](reference/output-envelope.md) -- JSON structure every tool produces
- [Workflow Schema](reference/workflow-schema.md) -- formal description of the workflow file format
