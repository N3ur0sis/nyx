/**
 * @file nyx_wf_expr.h
 * @brief Expression evaluator for the NYX workflow engine
 * @author Neur0sis (2025)
 *
 * Evaluates ${...} expressions against a workflow execution context.
 * Supports path resolution, pipe operations (filter, select, count,
 * first, flat), and boolean comparisons for conditional execution.
 *
 * Expression syntax:
 *   ${root.path.to.field}
 *   ${root.path | filter key == value}
 *   ${root.path | select key}
 *   ${root.path | count}
 *   ${root.path > 0}              (boolean)
 *
 * Roots: step_id, "vars", "each", "env"
 */

#ifndef NYX_WF_EXPR_H
#define NYX_WF_EXPR_H

#include "nyx_json.h"
#include "nyx_wf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Evaluates a single ${...} expression against the workflow context.
 *
 * @param expr      The expression string (with or without ${ } delimiters)
 * @param ctx       Workflow execution context (provides step results and vars)
 * @param each_item Current iteration item for for_each, or NULL
 * @return New JSON node with the resolved value (caller must free), or NULL
 */
nyx_json_t *nyx_wf_expr_eval(const char *expr, const nyx_wf_ctx_t *ctx,
                             const nyx_json_t *each_item);

/**
 * Evaluates a boolean expression (for `when` fields).
 * Handles comparisons like "${step.results.count > 0}".
 *
 * @param expr      The expression string
 * @param ctx       Workflow execution context
 * @param each_item Current iteration item, or NULL
 * @return 1 if true, 0 if false or on evaluation error
 */
int nyx_wf_expr_eval_bool(const char *expr, const nyx_wf_ctx_t *ctx, const nyx_json_t *each_item);

/**
 * Resolves all ${...} occurrences in a string, returning a new string.
 * Non-expression text is passed through verbatim.
 *
 * @param str       Input string (may contain zero or more ${...} expressions)
 * @param ctx       Workflow execution context
 * @param each_item Current iteration item, or NULL
 * @return New malloc'd string with expressions replaced, or NULL on error
 */
char *nyx_wf_expr_resolve_string(const char *str, const nyx_wf_ctx_t *ctx,
                                 const nyx_json_t *each_item);

/**
 * Scans a string for ${step_id...} references and collects unique step IDs.
 * Used during parsing to build the dependency graph.
 *
 * @param str       String to scan for references
 * @param ids       Output array of step ID strings (caller frees each + array)
 * @param count     Output number of unique IDs found
 * @return 0 on success, negative on error
 */
int nyx_wf_expr_scan_refs(const char *str, char ***ids, size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* NYX_WF_EXPR_H */
