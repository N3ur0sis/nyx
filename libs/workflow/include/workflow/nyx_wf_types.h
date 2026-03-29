/**
 * @file nyx_wf_types.h
 * @brief Core type definitions for the NYX workflow engine
 * @author Neur0sis (2025)
 *
 * Defines the data structures that represent a parsed workflow:
 * steps (nodes), the workflow container, and the runtime execution
 * context. These types are the foundation of the DAG-based engine.
 */

#ifndef NYX_WF_TYPES_H
#define NYX_WF_TYPES_H

#include <stddef.h>
#include "nyx_json.h"

/**
 * @name Step execution status codes
 * @{
 */
#define NYX_WF_STATUS_PENDING   0
#define NYX_WF_STATUS_DONE      1
#define NYX_WF_STATUS_SKIPPED   2
#define NYX_WF_STATUS_ERROR    -1
/** @} */

/**
 * @name Workflow error codes
 * @{
 */
#define NYX_WF_SUCCESS           0
#define NYX_WF_ERR_PARSE        -1
#define NYX_WF_ERR_VALIDATE     -2
#define NYX_WF_ERR_CYCLE        -3
#define NYX_WF_ERR_MISSING_REF  -4
#define NYX_WF_ERR_EXEC         -5
#define NYX_WF_ERR_EXPR         -6
#define NYX_WF_ERR_MEMORY       -7
#define NYX_WF_ERR_IO           -8
#define NYX_WF_ERR_PARAM        -9
/** @} */

/**
 * A single step (node) in the workflow DAG.
 */
typedef struct {
    char *id;                 /**< Unique identifier for this step */
    char *tool;               /**< NYX tool binary name (e.g. "nyx-portscan") */
    nyx_json_t *params;       /**< Raw param object (values may contain ${} expressions) */
    char *for_each;           /**< Expression evaluating to an iterable array, or NULL */
    char *when;               /**< Boolean expression for conditional execution, or NULL */
    nyx_json_t *meta;         /**< GUI metadata (position, color, etc.), or NULL */

    size_t *deps;             /**< Indices of upstream steps (computed from expressions) */
    size_t dep_count;         /**< Number of dependencies */
} nyx_wf_step_t;

/**
 * Complete parsed workflow definition.
 */
typedef struct {
    char *id;                 /**< Machine-readable workflow identifier */
    char *name;               /**< Human-readable name */
    char *version;            /**< Workflow version string */
    char *description;        /**< Workflow description */

    nyx_json_t *vars;         /**< Workflow-level variables (overridable from CLI) */

    nyx_wf_step_t *steps;    /**< Array of workflow steps */
    size_t step_count;        /**< Number of steps */
} nyx_workflow_t;

/**
 * Runtime execution context for a workflow run.
 */
typedef struct {
    const nyx_workflow_t *workflow;
    char *session_id;         /**< Shared session ID for all tool invocations */
    nyx_json_t **results;     /**< Per-step results (envelope or array for for_each) */
    int *status;              /**< Per-step execution status */
    char *bin_dir;            /**< Path to NYX binaries directory */

    size_t *exec_order;       /**< Topologically sorted step indices */
    size_t exec_order_len;    /**< Length of exec_order (== step_count if valid) */
} nyx_wf_ctx_t;

#endif /* NYX_WF_TYPES_H */
