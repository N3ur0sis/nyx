# for_each and when

## for_each -- Iteration

The `for_each` field runs a step once per element in an array. The array is produced by an expression, and within the step the current element is available as `${each}`.

### Example: Scan Each Alive Host

```json
{
  "id": "scan_all",
  "tool": "portscan",
  "for_each": "${sweep.results.hosts | filter alive == true}",
  "params": {
    "t": "${each.ip}",
    "P": "100"
  }
}
```

This evaluates the `for_each` expression to get an array of alive hosts. For each host, it runs `portscan` with `${each.ip}` resolved to that host's IP.

### Result Shape

When a step uses `for_each`, its result is an **array** of output envelopes (one per iteration) instead of a single envelope. Downstream expressions that reference the step get this array.

### Accessing for_each Results

Use `| flat` to merge arrays from iterations, or `| select` to extract a specific field:

```
${scan_all.results | flat}                -- all port results merged
${scan_all.results | select open_count}   -- array of open_count values
```

### Empty Arrays

If the `for_each` expression evaluates to an empty array, the step produces an empty result array and is marked as done (not skipped).

## when -- Conditional Execution

The `when` field is a boolean expression. If it evaluates to false, the step is skipped entirely.

### Example: Only Scan if Hosts Are Alive

```json
{
  "id": "scan_all",
  "tool": "portscan",
  "when": "${sweep.results.alive_count > 0}",
  "for_each": "${sweep.results.hosts | filter alive == true}",
  "params": {
    "t": "${each.ip}",
    "P": "100"
  }
}
```

### Skipped Steps

A skipped step:

- Has its status set to `skipped` in the workflow output
- Produces a `null` result
- Does not block downstream steps that depend on it, but those steps will receive `null` when they reference the skipped step's results

### Combining for_each and when

When both `for_each` and `when` are present on a step, `when` is evaluated first. If false, the entire step (including all iterations) is skipped. The `when` expression does not have access to `${each}` because it runs before iteration begins.

## Error Handling

If a tool invocation fails during `for_each`, the individual iteration is recorded with status `error` and the error details from the tool's structured error system. Other iterations may still succeed. The step's overall result array contains both successful and failed envelopes.

If a non-iterated step fails, it is recorded with status `error`. Downstream steps still execute unless their `when` condition evaluates to false based on the error state.
