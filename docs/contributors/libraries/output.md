# libs/output -- Output and JSON Library

**Header directory:** `libs/output/include/output/`

Provides the structured output envelope system (JSON envelopes, sessions) and a zero-dependency JSON builder/parser.

## nyx_json.h -- JSON Builder and Parser

A minimal, self-contained JSON library using a tagged-union tree model.

### Types

Seven JSON types: `NYX_JSON_NULL`, `NYX_JSON_BOOL`, `NYX_JSON_INT`, `NYX_JSON_DOUBLE`, `NYX_JSON_STRING`, `NYX_JSON_ARRAY`, `NYX_JSON_OBJECT`.

### Builder Functions

| Function | Purpose |
|----------|---------|
| `nyx_json_object()` | Create an empty JSON object |
| `nyx_json_array()` | Create an empty JSON array |
| `nyx_json_string(val)` | Create a string node |
| `nyx_json_int(val)` | Create an integer node |
| `nyx_json_real(val)` | Create a double node |
| `nyx_json_bool(val)` | Create a boolean node |
| `nyx_json_null()` | Create a null node |
| `nyx_json_set(obj, key, val)` | Add/replace a key in an object |
| `nyx_json_append(arr, val)` | Append an element to an array |
| `nyx_json_free(node)` | Free a JSON tree |

### Serializer

| Function | Purpose |
|----------|---------|
| `nyx_json_serialize(node, indent)` | Serialize to a `malloc`'d string (caller frees) |
| `nyx_json_write_file(node, path, indent)` | Write JSON to a file |

### Parser

| Function | Purpose |
|----------|---------|
| `nyx_json_parse(str)` | Parse a JSON string into a tree |
| `nyx_json_parse_file(path)` | Parse a JSON file into a tree |

### Accessors

| Function | Purpose |
|----------|---------|
| `nyx_json_type(node)` | Get the type of a node |
| `nyx_json_get_string(node)` | Get string value |
| `nyx_json_get_int(node)` | Get integer value |
| `nyx_json_get_real(node)` | Get double value |
| `nyx_json_get_bool(node)` | Get boolean value |
| `nyx_json_get(obj, key)` | Look up a key in an object |
| `nyx_json_length(node)` | Get array/object length |
| `nyx_json_at(arr, index)` | Get array element by index |

### Usage Pattern

```c
nyx_json_t *obj = nyx_json_object();
nyx_json_set(obj, "name", nyx_json_string("pingsweep"));
nyx_json_set(obj, "count", nyx_json_int(42));

char *str = nyx_json_serialize(obj, 2);
printf("%s\n", str);
free(str);

nyx_json_free(obj);
```

## nyx_output.h -- Structured Output

Standardized JSON envelope for all tool output, with session-based persistence and mode control.

### Lifecycle

| Function | Purpose |
|----------|---------|
| `nyx_output_init(tool, module, version)` | Create a new output context |
| `nyx_output_finish(ctx)` | Finalize: write to stdout (JSON mode) and/or session file |
| `nyx_output_free(ctx)` | Free all context resources |
| `nyx_output_build_envelope(ctx)` | Build the envelope as a JSON tree without writing it |

### Session Management

| Function | Purpose |
|----------|---------|
| `nyx_output_set_session(ctx, id)` | Join an existing session |
| `nyx_output_new_session(ctx)` | Create a new session with random ID |
| `nyx_output_get_session_id(ctx)` | Get the current session ID |
| `nyx_output_load_results(session_id, tool)` | Load a tool's results from a session |

### Mode Control

| Function | Purpose |
|----------|---------|
| `nyx_output_set_json_stdout(ctx, 1)` | Enable JSON output to stdout |
| `nyx_output_is_json_mode(ctx)` | Check if JSON mode is active |
| `nyx_output_has_structured_sink(ctx)` | Check if any structured output sink is enabled |
| `nyx_output_set_capture_path(ctx, path)` | Write envelope to a capture file |
| `nyx_output_argv_has_json(argc, argv)` | Check raw argv for `-J` / `--json` |

### Envelope Population

| Function | Purpose |
|----------|---------|
| `nyx_output_set_config(ctx, json)` | Set the `config` section (ownership transfers) |
| `nyx_output_set_results(ctx, json)` | Set the `results` section (ownership transfers) |
| `nyx_output_set_status(ctx, str)` | Set status: `"success"`, `"error"`, `"partial"` |
| `nyx_output_set_error_from_ctx(ctx)` | Set error from `nyx_error` context |
| `nyx_output_emit_error(ctx)` | Set error + status + finish in one call |
| `nyx_output_set_error_msg(ctx, msg)` | Set a custom error message |
| `nyx_output_emit_error_msg(ctx, msg)` | Custom error + status + finish in one call |

### CLI Helper

```c
nyx_output_ctx_t *out = nyx_output_from_cli("pingsweep", "phobos", "1.0",
                                             has_json_flag, session_id);
```

Creates a pre-configured output context from CLI flags.

### Usage in a Command Layer

```c
int my_invoke(const nyx_json_t *params, nyx_output_ctx_t *out)
{
    /* Parse params, build config echo */
    nyx_json_t *jcfg = nyx_json_object();
    /* ... populate jcfg ... */
    nyx_output_set_config(out, jcfg);

    /* Run tool implementation */
    int rc = my_tool_run(&cfg, &result);
    if (rc != 0) {
        nyx_output_set_error_from_ctx(out);
        return rc;
    }

    /* Build results */
    nyx_json_t *results = nyx_json_object();
    /* ... populate results ... */
    nyx_output_set_results(out, results);
    nyx_output_set_status(out, "success");
    return 0;
}
```
