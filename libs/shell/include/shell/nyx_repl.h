/**
 * @file nyx_repl.h
 * @brief Shared interactive shell (REPL) framework for the NYX framework
 * @author Neur0sis (2025)
 *
 * Provides a reusable interactive Read-Eval-Print Loop that both the
 * master `nyx` binary and individual tool shells consume.  Features:
 *
 *   - Named command registration with usage/help
 *   - Persistent history (~/.nyx/<name>_history)
 *   - Context switching (e.g. nyx> → nyx:pingsweep>)
 *   - Quoted/escaped tokenization
 *   - Built-in: help, history, exit/quit, back
 *
 * Layers that use this:
 *   - `nyx` master shell:  registers workflow, session, info, and per-tool
 *     context-switch commands.
 *   - Tool shells (e.g. nyx-pingsweep):  register tool-specific commands
 *     such as "scan", "list", etc.
 */

#ifndef NYX_REPL_H
#define NYX_REPL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque REPL handle */
typedef struct nyx_repl nyx_repl_t;

/**
 * Command handler signature.
 * @param argc  Number of arguments (including the command name at argv[0])
 * @param argv  NULL-terminated argument vector
 * @param data  User data set via nyx_repl_set_userdata()
 * @return 0 on success, non-zero on error (does NOT exit the REPL)
 */
typedef int (*nyx_repl_handler_fn)(int argc, char **argv, void *data);

/**
 * Tab-completion type for a flag's value argument.
 */
typedef enum {
    NYX_COMPL_NONE  = 0,  /**< No value completion */
    NYX_COMPL_IFACE,      /**< Complete network interface names */
    NYX_COMPL_FILE,       /**< Complete filesystem paths */
    NYX_COMPL_TOOL,       /**< Complete registered tool names */
} nyx_compl_type_t;

/**
 * Flag descriptor for tab-completion metadata.
 */
typedef struct nyx_repl_flag {
    const char      *name;       /**< Flag string (e.g. "-c", "--cidr") */
    nyx_compl_type_t compl_type; /**< Value completion type */
} nyx_repl_flag_t;

/**
 * Command definition.
 *
 * Fields:
 *   name        – Command token typed by the user (e.g. "scan").
 *   usage       – One-line synopsis shown in the help summary
 *                 (e.g. "scan -t <ip> [-p range]").
 *   description – Short description for the help summary.
 *   help        – Optional multi-line detailed help displayed by
 *                 "help <command>".  NULL means no extended help.
 *   handler     – Function called when the command is invoked.
 *   flags       – Optional array of flag descriptors for tab-completion.
 *                 Points to static data (not owned, not freed).
 *   flag_count  – Number of entries in the flags array.
 */
typedef struct nyx_repl_cmd {
    const char *name;
    const char *usage;
    const char *description;
    const char *help;
    nyx_repl_handler_fn handler;
    const nyx_repl_flag_t *flags;
    size_t                 flag_count;
} nyx_repl_cmd_t;

/* ---- Lifecycle ---- */

/**
 * Create a new REPL.
 * @param name      Shell name used in prompt prefix and history filename
 *                  (e.g. "nyx", "pingsweep").  Prompt becomes "name> ".
 * @return Handle, or NULL on allocation failure
 */
nyx_repl_t *nyx_repl_create(const char *name);

/**
 * Destroy a REPL, freeing all resources including history.
 */
void nyx_repl_free(nyx_repl_t *repl);

/* ---- Configuration ---- */

/**
 * Register a single command.  The command struct is copied internally.
 */
void nyx_repl_add_cmd(nyx_repl_t *repl, const nyx_repl_cmd_t *cmd);

/**
 * Register an array of commands at once.
 */
void nyx_repl_add_cmds(nyx_repl_t *repl, const nyx_repl_cmd_t *cmds,
                        size_t count);

/**
 * Set a fallback handler invoked when no registered command matches.
 * Useful for the master shell to check if the token is a known tool name.
 */
void nyx_repl_set_fallback(nyx_repl_t *repl, nyx_repl_handler_fn fn);

/**
 * Attach opaque user data retrievable by command handlers.
 */
void nyx_repl_set_userdata(nyx_repl_t *repl, void *data);
void *nyx_repl_get_userdata(const nyx_repl_t *repl);

/**
 * Set or clear a sub-context label.  When set, the prompt becomes
 * "name:context> " instead of "name> ".  Pass NULL to clear.
 */
void nyx_repl_set_context(nyx_repl_t *repl, const char *context);

/**
 * Return the current context label, or NULL if none.
 */
const char *nyx_repl_get_context(const nyx_repl_t *repl);

/**
 * Set a welcome message printed once when the REPL starts.
 * Pass NULL to suppress.
 */
void nyx_repl_set_welcome(nyx_repl_t *repl, const char *message);

/* ---- Execution ---- */

/**
 * Enter the interactive REPL loop.  Blocks until the user types
 * "exit", "quit", or sends EOF.
 * @return 0 on clean exit, non-zero on error
 */
int nyx_repl_run(nyx_repl_t *repl);

/**
 * Signal the REPL to exit after the current command completes.
 * Safe to call from inside a command handler.
 */
void nyx_repl_request_exit(nyx_repl_t *repl);

/* ---- Utilities (usable outside the REPL loop) ---- */

/**
 * Tokenize a command line string into an argc/argv pair.
 * Handles single/double quotes and backslash escapes.
 * @param line      Input string (not modified)
 * @param argc_out  Receives argument count
 * @return Heap-allocated argv (caller must free with nyx_repl_free_tokens)
 */
char **nyx_repl_tokenize(const char *line, int *argc_out);

/**
 * Free a token array returned by nyx_repl_tokenize.
 */
void nyx_repl_free_tokens(char **argv, int argc);

#ifdef __cplusplus
}
#endif

#endif /* NYX_REPL_H */
