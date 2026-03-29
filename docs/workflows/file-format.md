# Workflow File Format

Workflow files are JSON with three top-level sections: `workflow` (metadata), `vars` (variables), and `steps` (the DAG).

## Minimal Example

```json
{
  "steps": [
    {
      "id": "scan",
      "tool": "pingsweep",
      "params": { "c": "192.168.1.0/24" }
    }
  ]
}
```

## Full Structure

```json
{
  "workflow": {
    "id": "net-discovery",
    "name": "Network Discovery",
    "version": "1.0",
    "description": "Sweep a subnet then port-scan alive hosts"
  },
  "vars": {
    "subnet": "192.168.1.0/24",
    "top_ports": 100
  },
  "steps": [
    { ... },
    { ... }
  ]
}
```

### `workflow` (optional)

Metadata about the workflow. All fields are optional strings.

| Field | Description |
|-------|-------------|
| `id` | Machine-readable identifier (used in log output) |
| `name` | Human-readable name |
| `version` | Version string |
| `description` | One-line summary |

### `vars` (optional)

A flat JSON object of workflow-level variables. Values can be strings, numbers, or booleans. Variables are referenced in step parameters via `${vars.key}` and can be overridden from the command line with `--var key=value`.

### `steps` (required)

A non-empty array of step objects. Each step represents one tool invocation.

## Step Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | yes | Unique identifier for this step (used in expressions and logs) |
| `tool` | string | yes | Tool name. Use the short name (`pingsweep`) or the binary name (`nyx-pingsweep`) |
| `params` | object | no | Parameters passed to the tool. Values may contain `${...}` expressions |
| `for_each` | string | no | Expression that evaluates to an array. The step runs once per element |
| `when` | string | no | Boolean expression. If false, the step is skipped |
| `meta` | object | no | Opaque metadata for GUI tools (layout, color, etc.) |

### Tool Names

The engine strips a `nyx-` prefix when looking up tools, so both `"pingsweep"` and `"nyx-pingsweep"` work. Use the short name for clarity.

### Parameter Keys

Each tool defines which parameter keys it reads. See the [Tool Reference](../tools/overview.md) for the exact keys accepted by each tool.

Parameter values that are strings may contain `${...}` expressions. If the entire value is a single expression (e.g. `"${vars.subnet}"`), the resolved JSON value is injected directly -- this preserves types like numbers and arrays. If the value contains a mix of text and expressions, the result is always a string.

## Dependency Resolution

You do not declare dependencies between steps. The engine scans all `${...}` expressions in `params`, `for_each`, and `when` for step ID references and builds the dependency graph automatically.

For example, if step B has `"t": "${stepA.results.ip}"`, step B depends on step A. The engine executes steps in topological order computed via Kahn's algorithm.

## Validation

Before execution the engine checks:

- Every step has a unique, non-empty `id`
- Every step has a non-empty `tool`
- All referenced step IDs exist in the workflow
- The dependency graph has no cycles

The engine does **not** validate that tool names exist in the registry or that expressions are syntactically correct until execution time.
