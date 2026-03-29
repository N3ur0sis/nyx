/**
 * @file nyx_workflow.h
 * @brief Public API for the NYX workflow engine
 * @author Neur0sis (2025)
 *
 * Top-level interface for loading, validating, and executing workflow
 * files. This is the only header most consumers need to include.
 *
 * Usage:
 *   nyx_workflow_t *wf = nyx_wf_load_file("workflow.json");
 *   int rc = nyx_wf_validate(wf);
 *   nyx_wf_ctx_t *ctx = nyx_wf_ctx_create(wf, session_id, bin_dir);
 *   nyx_wf_run(ctx);
 *   nyx_json_t *results = nyx_wf_get_results(ctx);
 *   nyx_wf_ctx_free(ctx);
 *   nyx_wf_free(wf);
 */

#ifndef NYX_WORKFLOW_H
#define NYX_WORKFLOW_H

#include "nyx_wf_types.h"
#include "nyx_wf_expr.h"
#include "nyx_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Loading ---- */

/**
 * Load a workflow from a JSON file on disk.
 * @param path  Path to the workflow JSON file
 * @return Parsed workflow or NULL on error
 */
nyx_workflow_t *nyx_wf_load_file(const char *path);

/**
 * Parse a workflow from a pre-parsed JSON tree.
 * @param root  Root JSON object of the workflow
 * @return Parsed workflow or NULL on error
 */
nyx_workflow_t *nyx_wf_parse_json(const nyx_json_t *root);

/* ---- Validation ---- */

/**
 * Validate a workflow (unique IDs, valid refs, acyclicity).
 * @return NYX_WF_SUCCESS or an error code
 */
int nyx_wf_validate(const nyx_workflow_t *wf);

/* ---- Execution ---- */

/**
 * Create a runtime context for executing a workflow.
 * @param wf         Parsed workflow (must remain valid for the context's lifetime)
 * @param session_id Shared session ID, or NULL for no session
 * @param bin_dir    Path to NYX binaries, or NULL for PATH lookup
 * @return Context or NULL on error
 */
nyx_wf_ctx_t *nyx_wf_ctx_create(const nyx_workflow_t *wf,
                                  const char *session_id,
                                  const char *bin_dir);

/**
 * Execute all steps in topological order.
 * @return NYX_WF_SUCCESS or an error code
 */
int nyx_wf_run(nyx_wf_ctx_t *ctx);

/**
 * Collect all step results into a single JSON object.
 * @return New JSON object (caller must free) mapping step IDs to results
 */
nyx_json_t *nyx_wf_get_results(const nyx_wf_ctx_t *ctx);

/**
 * Override a workflow variable.
 * @param wf    Workflow to modify
 * @param key   Variable name
 * @param value Variable value (JSON string)
 */
void nyx_wf_set_var(nyx_workflow_t *wf, const char *key, const char *value);

/* ---- Cleanup ---- */

void nyx_wf_ctx_free(nyx_wf_ctx_t *ctx);
void nyx_wf_free(nyx_workflow_t *wf);

/* ---- Graph utilities ---- */

/**
 * Topological sort of workflow steps using Kahn's algorithm.
 * @param wf         The workflow
 * @param order      Output array of step indices in execution order
 * @param order_len  Number of entries in order
 * @return NYX_WF_SUCCESS, NYX_WF_ERR_CYCLE, or other error
 */
int nyx_wf_topo_sort(const nyx_workflow_t *wf,
                      size_t **order, size_t *order_len);

#ifdef __cplusplus
}
#endif

#endif /* NYX_WORKFLOW_H */
