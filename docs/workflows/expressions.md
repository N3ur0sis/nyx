# Expression Language

Expressions use `${...}` syntax to reference data from previous step results, workflow variables, and loop iterators. They appear in step `params`, `for_each`, and `when` fields.

## Syntax

```
${root.path.to.field}
${root.path.to.field | pipe_op}
${root.path.to.field > 0}
```

## Roots

An expression starts with a root, followed by a dot-separated path.

| Root | Description |
|------|-------------|
| `vars` | Workflow-level variables from the `vars` section |
| *step_id* | Results from a completed step (e.g. `sweep.results.hosts`) |
| `each` | Current iteration item inside a `for_each` step |

### Step Result Navigation

When referencing a step, the path navigates into that step's output envelope:

```
${sweep.results.hosts}       -- the hosts array from step "sweep"
${sweep.results.alive_count} -- the alive_count field
${scan.results.ports}        -- the ports array from step "scan"
```

The envelope structure is `{ "results": { ... }, "status": "...", "error": ... }`. Most expressions navigate into `results`.

### Array Indexing

Use `[N]` for zero-based array indexing:

```
${sweep.results.hosts[0].ip}   -- IP of the first host
```

## Pipe Operations

Pipe operations transform the value using `| operator [args]`:

| Operator | Args | Description | Example |
|----------|------|-------------|---------|
| `filter` | `key == value` or `key != value` | Keep array items matching the condition | `${sweep.results.hosts \| filter alive == true}` |
| `select` | `key` | Extract one field from each array item | `${sweep.results.hosts \| select ip}` |
| `count` | (none) | Return the length of an array | `${sweep.results.hosts \| count}` |
| `first` | (none) | Return the first element | `${sweep.results.hosts \| first}` |
| `flat` | (none) | Flatten nested arrays into a single array | `${scan_all.results \| flat}` |

### Chaining Pipes

Pipes can be chained left to right:

```
${sweep.results.hosts | filter alive == true | select ip}
```

This filters for alive hosts, then extracts their IPs into a string array.

## Boolean Expressions

Used in `when` fields to control conditional execution. Supported comparison operators:

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `>` | Greater than |
| `<` | Less than |
| `>=` | Greater than or equal |
| `<=` | Less than or equal |

Examples:

```
${sweep.results.alive_count > 0}
${scan.results.open_count == 0}
```

### Truthiness

When a `when` expression does not contain a comparison operator, it is evaluated for truthiness:

- `0`, `null`, empty string, empty array → **false**
- Everything else → **true**

## String Substitution vs Value Injection

If a parameter value is **entirely** a single expression (the string starts with `${` and ends with `}`), the resolved JSON value is injected directly. This preserves types:

```json
{ "T": "${vars.threads}" }
```

If `vars.threads` is `8`, the parameter receives the integer `8`, not the string `"8"`.

If the value mixes text and expressions, the result is always a string:

```json
{ "c": "${each.ip}/32" }
```

This concatenates the resolved IP with `/32` to produce a string like `"10.0.0.5/32"`.

## Current Limitations

- The `env` root (for environment variables) is declared in the header but not currently resolved at runtime. Use `vars` with `--var` overrides instead.
- Nested `${...}` expressions (expressions inside expressions) are not supported.
- Pipe arguments only support `==` and `!=` comparisons (not `>`, `<`, etc.) in `filter`.
