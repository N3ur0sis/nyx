/**
 * @file nyx_output.h
 * @brief Structured output and session management for the NYX framework
 * @author Neur0sis (2025)
 *
 * Provides a standardized JSON envelope for all tool output, session-based
 * file persistence, and CLI integration helpers. Every tool run produces
 * a consistent envelope:
 *
 *   { "nyx": { ... }, "config": { ... }, "status": "...",
 *     "error": null, "results": { ... } }
 *
 * Session directories live under ~/.nyx/sessions/<session-id>/ and store
 * per-tool result files (<tool>.nyx.json) for tool chaining and workflow
 * automation.
 */

#ifndef NYX_OUTPUT_H
#define NYX_OUTPUT_H

#include "nyx_json.h"

typedef struct nyx_output_ctx nyx_output_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Lifecycle ---- */

/**
 * Creates a new output context for a tool run.
 * @param tool    Tool name (e.g. "nyx-pingsweep")
 * @param module  Module name (e.g. "phobos")
 * @param version Version string (e.g. "0.1.0")
 * @return Context pointer or NULL on allocation failure
 */
nyx_output_ctx_t *nyx_output_init(const char *tool, const char *module, const char *version);

/**
 * Finalizes the output: serializes the envelope to stdout (if json mode)
 * and/or writes the session file (if session is set). Records duration.
 * @return 0 on success, -1 on error
 */
int nyx_output_finish(nyx_output_ctx_t *ctx);

/**
 * Frees all resources held by the output context.
 */
void nyx_output_free(nyx_output_ctx_t *ctx);

/* ---- Session ---- */

/**
 * Joins an existing session by ID. The session directory is created if
 * it doesn't exist yet.
 * @return 0 on success, -1 on error
 */
int nyx_output_set_session(nyx_output_ctx_t *ctx, const char *session_id);

/**
 * Creates a new session with a random 8-char hex ID (from /dev/urandom).
 * @return 0 on success, -1 on error
 */
int nyx_output_new_session(nyx_output_ctx_t *ctx);

/**
 * Returns the current session ID, or NULL if no session is set.
 */
const char *nyx_output_get_session_id(const nyx_output_ctx_t *ctx);

/* ---- Mode control ---- */

/**
 * Enables or disables JSON-to-stdout mode. When enabled, nyx_output_finish()
 * writes the envelope to stdout instead of (or in addition to) a file.
 */
void nyx_output_set_json_stdout(nyx_output_ctx_t *ctx, int enabled);

/**
 * Returns 1 if stdout is reserved for JSON output.
 * Tools use this to suppress human-readable stdout/stderr UX.
 */
int nyx_output_is_json_mode(const nyx_output_ctx_t *ctx);

/**
 * Returns 1 if the run has any structured-output sink enabled:
 * JSON stdout, session file, or capture file.
 */
int nyx_output_has_structured_sink(const nyx_output_ctx_t *ctx);

/**
 * Enables writing the final structured envelope to an explicit capture file.
 * Used by orchestrators that want human-readable live output plus machine
 * capture separately.
 */
void nyx_output_set_capture_path(nyx_output_ctx_t *ctx, const char *path);

/* ---- Envelope fields ---- */

/**
 * Sets the "config" section of the envelope. Ownership of the JSON tree
 * transfers to the context (freed on nyx_output_free).
 */
void nyx_output_set_config(nyx_output_ctx_t *ctx, nyx_json_t *config);

/**
 * Sets the "results" section. Ownership transfers to the context.
 */
void nyx_output_set_results(nyx_output_ctx_t *ctx, nyx_json_t *results);

/**
 * Sets the "status" string (e.g. "success", "error", "partial").
 */
void nyx_output_set_status(nyx_output_ctx_t *ctx, const char *status);

/**
 * Sets the "error" section from the current nyx_error context.
 * Typically called when a tool operation fails.
 */
void nyx_output_set_error_from_ctx(nyx_output_ctx_t *ctx);

/**
 * Convenience: sets error from nyx_error context, sets status to "error",
 * and calls finish in one shot. Use for early-exit error paths.
 */
void nyx_output_emit_error(nyx_output_ctx_t *ctx);

/**
 * Emit a JSON error envelope with a custom message (no nyx_error required).
 * Sets status to "error", populates the error field, and finishes.
 */
void nyx_output_set_error_msg(nyx_output_ctx_t *ctx, const char *message);
void nyx_output_emit_error_msg(nyx_output_ctx_t *ctx, const char *message);

/**
 * Build the full structured envelope as a JSON tree WITHOUT writing it
 * anywhere.  Useful for in-process callers (workflow engine) that need
 * the envelope as a data structure rather than serialised output.
 *
 * Ownership of config/results/error transfers into the returned tree,
 * same as nyx_output_finish().  Caller must nyx_json_free() the result.
 *
 * @return Envelope JSON object, or NULL on allocation failure
 */
nyx_json_t *nyx_output_build_envelope(nyx_output_ctx_t *ctx);

/**
 * Scan raw argv for -J/--json flag. Use before nyx_output_from_cli is created
 * to decide whether to emit minimal JSON on early parse failures.
 */
int nyx_output_argv_has_json(int argc, char **argv);

/* ---- Reading previous output ---- */

/**
 * Loads a tool's results from a session directory.
 * @param session_id Session ID
 * @param tool_name  Tool name (filename stem, e.g. "pingsweep")
 * @return Parsed JSON tree or NULL on error. Caller must nyx_json_free().
 */
nyx_json_t *nyx_output_load_results(const char *session_id, const char *tool_name);

/* ---- CLI helper ---- */

/**
 * Scans parsed CLI results for --json/-J and --session/-S flags and
 * configures an output context accordingly.
 * @param tool    Tool name
 * @param module  Module name
 * @param version Version string
 * @param has_json    1 if --json/-J flag was provided
 * @param session_id  Value of --session/-S flag, or NULL
 * @return Configured context, or NULL on error
 */
nyx_output_ctx_t *nyx_output_from_cli(const char *tool, const char *module, const char *version,
                                      int has_json, const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* NYX_OUTPUT_H */
