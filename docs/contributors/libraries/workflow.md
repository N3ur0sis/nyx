# libs/workflow -- Workflow Engine Library

**Header directory:** `libs/workflow/include/workflow/`

The DAG-based workflow engine: parses JSON workflow files, resolves dependencies, evaluates expressions, and executes tool invocations in topological order.

## nyx_workflow.h -- Public API

Top-level entry point for loading and running workflows.

### Lifecycle

| Function | Purpose |
|----------|---------|
| `nyx_wf_load_file(path)` | Load and parse a workflow JSON file |
| `nyx_wf_parse_json(root)` | Parse from a pre-parsed JSON tree |
| `nyx_wf_validate(wf)` | Validate: unique IDs, valid refs, no cycles |
| `nyx_wf_ctx_create(wf, session_id, bin_dir)` | Create execution context |
| `nyx_wf_run(ctx)` | Execute all steps in order |
| `nyx_wf_get_results(ctx)` | Collect results into a JSON object |
| `nyx_wf_set_var(wf, key, value)` | Override a workflow variable |
| `nyx_wf_ctx_free(ctx)` | Free execution context |
| `nyx_wf_free(wf)` | Free parsed workflow |

### Graph Utilities

| Function | Purpose |
|----------|---------|
| `nyx_wf_topo_sort(wf, order, len)` | Topological sort via Kahn's algorithm |

### Usage Pattern

```c
nyx_workflow_t *wf = nyx_wf_load_file("workflow.json");
nyx_wf_set_var(wf, "subnet", "10.0.0.0/24");

int rc = nyx_wf_validate(wf);
if (rc != NYX_WF_SUCCESS) { /* handle error */ }

nyx_wf_ctx_t *ctx = nyx_wf_ctx_create(wf, session_id, NULL);
nyx_wf_run(ctx);

nyx_json_t *results = nyx_wf_get_results(ctx);
/* process results */
nyx_json_free(results);

nyx_wf_ctx_free(ctx);
nyx_wf_free(wf);
```

## nyx_wf_types.h -- Core Types

### Step Status

| Constant | Value | Meaning |
|----------|-------|---------|
| `NYX_WF_STATUS_PENDING` | 0 | Not yet executed |
| `NYX_WF_STATUS_DONE` | 1 | Completed successfully |
| `NYX_WF_STATUS_SKIPPED` | 2 | Skipped by `when` condition |
| `NYX_WF_STATUS_ERROR` | -1 | Failed |

### Error Codes

| Constant | Meaning |
|----------|---------|
| `NYX_WF_SUCCESS` | No error |
| `NYX_WF_ERR_PARSE` | JSON parsing failure |
| `NYX_WF_ERR_VALIDATE` | Validation failure (missing IDs, etc.) |
| `NYX_WF_ERR_CYCLE` | Dependency cycle detected |
| `NYX_WF_ERR_MISSING_REF` | Expression references a nonexistent step |
| `NYX_WF_ERR_EXEC` | Tool execution failure |
| `NYX_WF_ERR_EXPR` | Expression evaluation failure |

### Key Structures

**nyx_wf_step_t** -- a single step (node) in the DAG:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `char *` | Unique step identifier |
| `tool` | `char *` | Tool name |
| `params` | `nyx_json_t *` | Raw parameter object (may contain expressions) |
| `for_each` | `char *` | Array expression for iteration |
| `when` | `char *` | Boolean expression for conditional execution |
| `meta` | `nyx_json_t *` | GUI metadata |
| `deps` | `size_t *` | Computed dependency indices |
| `dep_count` | `size_t` | Number of dependencies |

**nyx_workflow_t** -- parsed workflow:

| Field | Type | Description |
|-------|------|-------------|
| `id`, `name`, `version`, `description` | `char *` | Metadata |
| `vars` | `nyx_json_t *` | Workflow variables |
| `steps` | `nyx_wf_step_t *` | Array of steps |
| `step_count` | `size_t` | Number of steps |

**nyx_wf_ctx_t** -- execution context:

| Field | Type | Description |
|-------|------|-------------|
| `workflow` | `const nyx_workflow_t *` | Reference to the parsed workflow |
| `session_id` | `char *` | Shared session ID |
| `results` | `nyx_json_t **` | Per-step result envelopes |
| `status` | `int *` | Per-step execution status |
| `exec_order` | `size_t *` | Topologically sorted step indices |

## nyx_wf_expr.h -- Expression Engine

Evaluates `${...}` expressions against a workflow execution context.

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_wf_expr_eval(expr, ctx, each)` | Evaluate expression to a JSON value |
| `nyx_wf_expr_eval_bool(expr, ctx, each)` | Evaluate as boolean (for `when`) |
| `nyx_wf_expr_resolve_string(str, ctx, each)` | Resolve all `${...}` in a string |
| `nyx_wf_expr_scan_refs(str, ids, count)` | Find step ID references (for dependency building) |

### How the Engine Uses Expressions

1. **Parsing**: `nyx_wf_expr_scan_refs()` scans all `params`, `for_each`, and `when` strings for `${step_id...}` patterns to build the dependency graph.

2. **Execution**: for each step in topological order:
   - Evaluate `when` with `nyx_wf_expr_eval_bool()`. Skip if false.
   - Evaluate `for_each` with `nyx_wf_expr_eval()`. Iterate if array.
   - For each parameter value, call `nyx_wf_expr_resolve_string()` to substitute expressions with resolved values.
   - Invoke the tool with the resolved parameter object.
   - Store the result envelope for downstream expressions.
