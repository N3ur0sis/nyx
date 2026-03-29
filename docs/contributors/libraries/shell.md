# libs/shell -- Interactive Shell Library

**Header directory:** `libs/shell/include/shell/`

Provides the shared REPL framework and the global tool registry. Both the master `nyx` binary and standalone tool shells are built on top of this library.

## nyx_repl.h -- REPL Framework

A reusable interactive shell with command registration, history, line editing (via linenoise), and tab completion.

### Key Types

- `nyx_repl_t` -- opaque REPL handle
- `nyx_repl_cmd_t` -- command definition (name, usage, description, help, handler, flags)
- `nyx_repl_flag_t` -- flag descriptor for tab completion (flag name + completion type)
- `nyx_compl_type_t` -- completion type enum

### Completion Types

| Value | Behavior |
|-------|----------|
| `NYX_COMPL_NONE` | No automatic value completion |
| `NYX_COMPL_IFACE` | Complete with network interface names |
| `NYX_COMPL_FILE` | Complete with filesystem paths |
| `NYX_COMPL_TOOL` | Complete with registered tool names |

### Lifecycle

| Function | Purpose |
|----------|---------|
| `nyx_repl_create(name)` | Create a REPL. Name is used in prompt and history file |
| `nyx_repl_free(repl)` | Destroy and free all resources |
| `nyx_repl_run(repl)` | Enter the interactive loop (blocks until exit) |
| `nyx_repl_request_exit(repl)` | Signal exit from within a handler |

### Configuration

| Function | Purpose |
|----------|---------|
| `nyx_repl_add_cmd(repl, cmd)` | Register a single command |
| `nyx_repl_add_cmds(repl, cmds, count)` | Register an array of commands |
| `nyx_repl_set_fallback(repl, fn)` | Handler for unrecognized commands |
| `nyx_repl_set_userdata(repl, data)` | Attach opaque data for handlers |
| `nyx_repl_get_userdata(repl)` | Retrieve attached data |
| `nyx_repl_set_context(repl, label)` | Set sub-context (changes prompt to `name:label>`) |
| `nyx_repl_get_context(repl)` | Get current context label |
| `nyx_repl_set_welcome(repl, msg)` | Set welcome message |

### Built-in Commands

Every REPL automatically provides:

| Command | Action |
|---------|--------|
| `help` | List registered commands |
| `help <cmd>` | Show detailed help for a command |
| `history` | Show command history |
| `clear` | Clear the terminal |
| `exit` / `quit` | Exit the REPL |
| `back` | Exit context (equivalent to exit in sub-contexts) |

### Tokenizer

```c
int argc;
char **argv = nyx_repl_tokenize("scan -c '10.0.0.0/24' -T 8", &argc);
/* argv = ["scan", "-c", "10.0.0.0/24", "-T", "8"], argc = 5 */
nyx_repl_free_tokens(argv, argc);
```

Handles single/double quotes and backslash escapes.

### Usage Pattern

```c
nyx_repl_t *repl = nyx_repl_create("mytool");
nyx_repl_set_welcome(repl, "Welcome. Type 'help' for commands.");
nyx_repl_add_cmds(repl, my_cmds, my_cmd_count);
nyx_repl_run(repl);
nyx_repl_free(repl);
```

## nyx_tool_registry.h -- Tool Registry

Global registry for in-process tool invocation. The workflow engine and master shell look up tools here instead of spawning binaries.

### Key Types

- `nyx_tool_entry_t` -- tool descriptor (name, module, version, description, invoke function, REPL commands, privilege requirements)
- `nyx_tool_invoke_fn` -- function signature: `int fn(const nyx_json_t *params, nyx_output_ctx_t *out)`

### Functions

| Function | Purpose |
|----------|---------|
| `nyx_tool_registry_add(entry)` | Register a tool (entry is copied) |
| `nyx_tool_registry_find(name)` | Look up a tool by name |
| `nyx_tool_registry_count()` | Number of registered tools |
| `nyx_tool_registry_at(index)` | Access tool by index |
| `nyx_tool_registry_cleanup()` | Free all entries |
| `nyx_tools_register_all()` | Bootstrap: register all built-in tools |

### Registration Pattern

Each tool defines a `*_register()` function called from `nyx_tools_register_all()`:

```c
void my_tool_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name          = "mytool",
        .module        = "phobos",
        .version       = "1.0",
        .description   = "One-line description",
        .invoke        = my_invoke,
        .cmds          = my_repl_cmds,
        .cmd_count     = MY_CMD_COUNT,
        .required_priv = NYX_PRIV_NONE
    });
}
```

### Invocation

The workflow engine and master shell both use:

```c
const nyx_tool_entry_t *tool = nyx_tool_registry_find("pingsweep");
if (tool) {
    nyx_output_ctx_t *out = nyx_output_init(tool->name, tool->module, tool->version);
    int rc = tool->invoke(params, out);
    nyx_json_t *envelope = nyx_output_build_envelope(out);
    nyx_output_free(out);
}
```
