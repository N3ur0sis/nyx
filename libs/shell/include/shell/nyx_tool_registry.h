/**
 * @file nyx_tool_registry.h
 * @brief Global tool registry for in-process tool invocation
 * @author Neur0sis (2025)
 *
 * Every NYX tool registers a command-layer entry at startup.  The
 * workflow engine and the interactive shell both look up tools here
 * instead of spawning external binaries.
 *
 * Registration pattern (in each tool's cmd.c):
 *
 *   static int my_invoke(const nyx_json_t *p, nyx_output_ctx_t *o) { ... }
 *   void my_tool_register(void) {
 *       nyx_tool_registry_add(&(nyx_tool_entry_t){
 *           .name = "pingsweep", .module = "phobos", .version = "1.0",
 *           .description = "ICMP ping sweep", .invoke = my_invoke
 *       });
 *   }
 *
 * The master binary and nyx-run call nyx_tools_register_all() once at
 * startup, which in turn calls each tool's registration function.
 */

#ifndef NYX_TOOL_REGISTRY_H
#define NYX_TOOL_REGISTRY_H

#include <stddef.h>

struct nyx_json;
typedef struct nyx_json nyx_json_t;

struct nyx_output_ctx;
typedef struct nyx_output_ctx nyx_output_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tool command-layer invoke function.
 *
 * Called by the workflow engine or the interactive shell to run a tool
 * in-process.  The function must:
 *   1. Parse relevant fields from @p params (JSON object).
 *   2. Call the tool's core implementation.
 *   3. Populate @p out with config, results, status, and error fields.
 *   4. NOT call nyx_output_finish() -- the caller decides when to flush.
 *
 * @param params  Resolved JSON parameter object (never NULL, may be empty)
 * @param out     Pre-initialized output context
 * @return 0 on success, non-zero on tool-level error
 */
typedef int (*nyx_tool_invoke_fn)(const nyx_json_t *params,
                                  nyx_output_ctx_t *out);

struct nyx_repl_cmd;

/**
 * Describes a registered tool.
 */
typedef struct {
    const char *name;        /**< Short tool name (e.g. "pingsweep") */
    const char *module;      /**< Module (e.g. "phobos") */
    const char *version;     /**< Version string */
    const char *description; /**< One-line description for help/listings */
    nyx_tool_invoke_fn invoke;

    const struct nyx_repl_cmd *cmds;  /**< REPL command table (shared, not owned) */
    size_t cmd_count;                 /**< Number of entries in cmds */

    int required_priv;       /**< nyx_priv_t bitmask needed to run this tool */
} nyx_tool_entry_t;

/* ---- Registry management ---- */

/**
 * Add a tool to the global registry.  The entry is copied internally.
 * @return 0 on success, -1 on error (duplicate name or allocation failure)
 */
int nyx_tool_registry_add(const nyx_tool_entry_t *entry);

/**
 * Look up a tool by short name.
 * @return Pointer to the registry entry, or NULL if not found.
 *         The pointer is valid until nyx_tool_registry_cleanup().
 */
const nyx_tool_entry_t *nyx_tool_registry_find(const char *name);

/** Number of registered tools. */
size_t nyx_tool_registry_count(void);

/** Access a tool by index (0 ≤ index < count). */
const nyx_tool_entry_t *nyx_tool_registry_at(size_t index);

/** Free all registry entries. */
void nyx_tool_registry_cleanup(void);

/* ---- Bootstrap ---- */

/**
 * Register all built-in tools.  Called once at program startup by
 * any binary that needs in-process tool access (nyx, nyx-run, etc.).
 *
 * Implementation is in a separate compilation unit that links against
 * all tool command libraries.
 */
void nyx_tools_register_all(void);

#ifdef __cplusplus
}
#endif

#endif /* NYX_TOOL_REGISTRY_H */
