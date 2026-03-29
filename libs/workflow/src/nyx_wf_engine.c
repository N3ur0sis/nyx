/**
 * @file nyx_wf_engine.c
 * @brief Execution engine for the NYX workflow runtime
 * @author Neur0sis (2025)
 *
 * Executes workflow steps in topological order via in-process tool
 * invocation through the global tool registry.  Handles expression
 * resolution, for_each fan-out, and conditional (when) execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nyx_wf_types.h"
#include "nyx_wf_expr.h"
#include "nyx_json.h"
#include "nyx_logger.h"
#include "nyx_output.h"
#include "nyx_priv.h"
#include "nyx_tool_registry.h"
#include "nyx_term.h"

int nyx_wf_topo_sort(const nyx_workflow_t *wf, size_t **order, size_t *order_len);

/* ====================================================================
 *  In-process tool invocation
 * ==================================================================== */

typedef struct {
    nyx_json_t *envelope;
    int rc;
} invoke_result_t;

/**
 * Invoke a tool in-process through the global registry.
 * Returns the full structured envelope and the tool's return code.
 */
static invoke_result_t invoke_tool(const char *tool_name, const nyx_json_t *params)
{
    invoke_result_t ir = {.envelope = NULL, .rc = -1};

    const nyx_tool_entry_t *tool = nyx_tool_registry_find(tool_name);
    if (!tool && strncmp(tool_name, "nyx-", 4) == 0)
        tool = nyx_tool_registry_find(tool_name + 4);
    if (!tool) {
        nyx_log(NYX_LOG_ERROR, "Tool '%s' not found in registry", tool_name);
        return ir;
    }

    nyx_output_ctx_t *out = nyx_output_init(tool->name, tool->module, tool->version);
    if (!out)
        return ir;

    ir.rc = tool->invoke(params, out);

    ir.envelope = nyx_output_build_envelope(out);
    nyx_output_free(out);
    return ir;
}

static int tool_reported_error(const nyx_json_t *envelope)
{
    if (!envelope)
        return 1;
    const nyx_json_t *st = nyx_json_get(envelope, "status");
    if (!st || nyx_json_type(st) != NYX_JSON_STRING)
        return 0;
    return strcmp(nyx_json_get_string(st), "error") == 0;
}

/* ====================================================================
 *  Parameter resolution
 * ==================================================================== */

static nyx_json_t *resolve_params(const nyx_json_t *params, const nyx_wf_ctx_t *ctx,
                                  const nyx_json_t *each_item)
{
    if (!params)
        return nyx_json_object();

    char *s = nyx_json_serialize(params, 0);
    if (!s)
        return nyx_json_object();

    nyx_json_t *resolved = nyx_json_object();
    const char *p = s;
    if (*p == '{')
        p++;

    while (*p) {
        while (*p && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t'))
            p++;
        if (*p == '}' || !*p)
            break;

        if (*p != '"')
            break;
        p++;
        const char *kstart = p;
        while (*p && *p != '"')
            p++;
        size_t klen = (size_t)(p - kstart);
        if (*p == '"')
            p++;

        char key[128];
        if (klen >= sizeof(key))
            break;
        memcpy(key, kstart, klen);
        key[klen] = '\0';

        while (*p && *p != ':')
            p++;
        if (*p == ':')
            p++;

        const nyx_json_t *orig = nyx_json_get(params, key);
        if (!orig)
            break;

        if (nyx_json_type(orig) == NYX_JSON_STRING) {
            const char *sv = nyx_json_get_string(orig);
            if (sv && strchr(sv, '$')) {
                const char *check = sv;
                while (*check == ' ')
                    check++;
                if (check[0] == '$' && check[1] == '{') {
                    const char *close = strchr(check + 2, '}');
                    if (close) {
                        const char *after = close + 1;
                        while (*after == ' ')
                            after++;
                        if (!*after) {
                            nyx_json_t *val = nyx_wf_expr_eval(sv, ctx, each_item);
                            nyx_json_set(resolved, key, val);
                            while (*p && *p != ',' && *p != '}') {
                                if (*p == '"') {
                                    p++;
                                    while (*p && *p != '"')
                                        p++;
                                    if (*p)
                                        p++;
                                } else
                                    p++;
                            }
                            continue;
                        }
                    }
                }
                char *rs = nyx_wf_expr_resolve_string(sv, ctx, each_item);
                nyx_json_set(resolved, key, nyx_json_string(rs ? rs : ""));
                free(rs);
            } else {
                nyx_json_set(resolved, key, nyx_json_string(sv ? sv : ""));
            }
        } else {
            char *vs = nyx_json_serialize(orig, 0);
            if (vs) {
                nyx_json_t *clone = nyx_json_parse(vs);
                free(vs);
                nyx_json_set(resolved, key, clone ? clone : nyx_json_null());
            }
        }

        while (*p && *p != ',' && *p != '}') {
            if (*p == '"') {
                p++;
                while (*p && *p != '"')
                    p++;
                if (*p)
                    p++;
            } else
                p++;
        }
    }

    free(s);
    return resolved;
}

/* ====================================================================
 *  Step execution
 * ==================================================================== */

static int execute_step(nyx_wf_ctx_t *ctx, size_t step_idx)
{
    const nyx_wf_step_t *step = &ctx->workflow->steps[step_idx];
    const char *wf_id = ctx->workflow->id ? ctx->workflow->id : "workflow";

    nyx_log(NYX_LOG_INFO, "[%s] Starting step '%s' (tool: %s)", wf_id, step->id, step->tool);

    if (step->when) {
        if (!nyx_wf_expr_eval_bool(step->when, ctx, NULL)) {
            nyx_log(NYX_LOG_INFO, "[%s] Step '%s' skipped (when: false)", wf_id, step->id);
            ctx->status[step_idx] = NYX_WF_STATUS_SKIPPED;
            return NYX_WF_SUCCESS;
        }
    }

    /* Verify tool exists before resolving params */
    {
        const nyx_tool_entry_t *t = nyx_tool_registry_find(step->tool);
        if (!t && strncmp(step->tool, "nyx-", 4) == 0)
            t = nyx_tool_registry_find(step->tool + 4);
        if (!t) {
            nyx_log(NYX_LOG_ERROR, "[%s] Step '%s': tool '%s' not registered", wf_id, step->id,
                    step->tool);
            ctx->status[step_idx] = NYX_WF_STATUS_ERROR;
            return NYX_WF_ERR_EXEC;
        }
    }

    /* ------ for_each fan-out ------ */
    if (step->for_each) {
        nyx_json_t *items = nyx_wf_expr_eval(step->for_each, ctx, NULL);
        if (!items || nyx_json_type(items) != NYX_JSON_ARRAY) {
            nyx_log(NYX_LOG_WARN,
                    "[%s] Step '%s' for_each resolved to "
                    "non-array, skipping",
                    wf_id, step->id);
            nyx_json_free(items);
            ctx->status[step_idx] = NYX_WF_STATUS_SKIPPED;
            return NYX_WF_SUCCESS;
        }

        size_t count = nyx_json_length(items);
        nyx_log(NYX_LOG_INFO,
                "[%s] Step '%s' expanding for_each "
                "(%zu iterations)",
                wf_id, step->id, count);

        nyx_json_t *iterations = nyx_json_array();
        int any_error = 0;

        for (size_t i = 0; i < count; i++) {
            const nyx_json_t *each_item = nyx_json_at(items, i);
            nyx_term_progress(step->id, i + 1, count);

            nyx_json_t *resolved = resolve_params(step->params, ctx, each_item);
            invoke_result_t ir = invoke_tool(step->tool, resolved);
            nyx_json_free(resolved);

            nyx_json_t *iter = nyx_json_object();

            char *es = nyx_json_serialize(each_item, 0);
            nyx_json_t *each_clone = es ? nyx_json_parse(es) : nyx_json_null();
            free(es);
            nyx_json_set(iter, "each", each_clone);

            if (ir.envelope) {
                const nyx_json_t *results = nyx_json_get(ir.envelope, "results");
                if (results) {
                    char *rs = nyx_json_serialize(results, 0);
                    nyx_json_t *rc = rs ? nyx_json_parse(rs) : nyx_json_object();
                    free(rs);
                    nyx_json_set(iter, "results", rc);
                }

                const nyx_json_t *st = nyx_json_get(ir.envelope, "status");
                if (st && nyx_json_type(st) == NYX_JSON_STRING)
                    nyx_json_set(iter, "status", nyx_json_string(nyx_json_get_string(st)));

                if (tool_reported_error(ir.envelope) || ir.rc != 0)
                    any_error = 1;

                nyx_json_free(ir.envelope);
            } else {
                nyx_json_set(iter, "status", nyx_json_string("error"));
                any_error = 1;
            }

            nyx_json_append(iterations, iter);
        }

        nyx_json_t *step_result = nyx_json_object();
        nyx_json_set(step_result, "status", nyx_json_string(any_error ? "error" : "success"));
        nyx_json_set(step_result, "iterations", iterations);

        nyx_json_t *merged = nyx_json_object();
        nyx_json_set(merged, "iteration_count", nyx_json_int((long)count));
        nyx_json_set(step_result, "results", merged);

        ctx->results[step_idx] = step_result;
        ctx->status[step_idx] = any_error ? NYX_WF_STATUS_ERROR : NYX_WF_STATUS_DONE;
        nyx_term_clear_status();
        nyx_json_free(items);

        if (any_error)
            nyx_log(NYX_LOG_ERROR,
                    "[%s] Step '%s' completed with errors "
                    "(%zu iterations)",
                    wf_id, step->id, count);
        else
            nyx_log(NYX_LOG_SUCCESS,
                    "[%s] Step '%s' completed "
                    "(%zu iterations)",
                    wf_id, step->id, count);

        return NYX_WF_SUCCESS;
    }

    /* ------ Single execution ------ */
    nyx_json_t *resolved = resolve_params(step->params, ctx, NULL);
    invoke_result_t ir = invoke_tool(step->tool, resolved);
    nyx_json_free(resolved);

    if (!ir.envelope) {
        nyx_log(NYX_LOG_ERROR, "[%s] Step '%s' failed (no output, rc %d)", wf_id, step->id, ir.rc);
        ctx->status[step_idx] = NYX_WF_STATUS_ERROR;
        return NYX_WF_ERR_EXEC;
    }

    ctx->results[step_idx] = ir.envelope;

    if (tool_reported_error(ir.envelope) || ir.rc != 0) {
        ctx->status[step_idx] = NYX_WF_STATUS_ERROR;

        const nyx_json_t *err = nyx_json_get(ir.envelope, "error");
        const char *emsg = "";
        if (err && nyx_json_type(err) == NYX_JSON_STRING)
            emsg = nyx_json_get_string(err);
        else if (err && nyx_json_type(err) == NYX_JSON_OBJECT) {
            const nyx_json_t *em = nyx_json_get(err, "message");
            if (em && nyx_json_type(em) == NYX_JSON_STRING)
                emsg = nyx_json_get_string(em);
        }

        nyx_log(NYX_LOG_ERROR, "[%s] Step '%s' error%s%s", wf_id, step->id, emsg[0] ? ": " : "",
                emsg);

        return NYX_WF_SUCCESS;
    }

    ctx->status[step_idx] = NYX_WF_STATUS_DONE;
    nyx_log(NYX_LOG_SUCCESS, "[%s] Step '%s' completed", wf_id, step->id);
    return NYX_WF_SUCCESS;
}

/* ====================================================================
 *  Public engine API
 * ==================================================================== */

nyx_wf_ctx_t *nyx_wf_ctx_create(const nyx_workflow_t *wf, const char *session_id,
                                const char *bin_dir)
{
    if (!wf)
        return NULL;

    nyx_wf_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->workflow = wf;

    if (session_id && session_id[0])
        ctx->session_id = strdup(session_id);
    if (bin_dir && bin_dir[0])
        ctx->bin_dir = strdup(bin_dir);

    ctx->results = calloc(wf->step_count, sizeof(nyx_json_t *));
    ctx->status = calloc(wf->step_count, sizeof(int));

    if (!ctx->results || !ctx->status) {
        free(ctx->session_id);
        free(ctx->bin_dir);
        free(ctx->results);
        free(ctx->status);
        free(ctx);
        return NULL;
    }

    int ret = nyx_wf_topo_sort(wf, &ctx->exec_order, &ctx->exec_order_len);
    if (ret != NYX_WF_SUCCESS) {
        free(ctx->session_id);
        free(ctx->bin_dir);
        free(ctx->results);
        free(ctx->status);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * Pre-flight: check if any workflow step requires elevated privileges
 * that we don't currently have.  Warns the user before execution starts.
 */
static int preflight_privilege_check(const nyx_wf_ctx_t *ctx)
{
    if (geteuid() == 0)
        return 0;

    const nyx_workflow_t *wf = ctx->workflow;
    int needs_priv = 0;
    int printed_header = 0;

    for (size_t i = 0; i < wf->step_count; i++) {
        const char *tool_name = wf->steps[i].tool;
        const nyx_tool_entry_t *t = nyx_tool_registry_find(tool_name);
        if (!t && strncmp(tool_name, "nyx-", 4) == 0)
            t = nyx_tool_registry_find(tool_name + 4);
        if (!t)
            continue;

        if (t->required_priv != NYX_PRIV_NONE && !nyx_priv_check((nyx_priv_t)t->required_priv)) {
            if (!printed_header) {
                nyx_log(NYX_LOG_WARN, "Workflow requires elevated privileges for:");
                printed_header = 1;
            }
            nyx_log(NYX_LOG_WARN, "  - %s (step: %s) [%s]", t->name, wf->steps[i].id,
                    nyx_priv_label((nyx_priv_t)t->required_priv));
            needs_priv = 1;
        }
    }

    if (needs_priv) {
        nyx_log(NYX_LOG_WARN, "Run with: sudo nyx run <workflow>");
        nyx_log(NYX_LOG_WARN, "Continuing anyway -- privileged steps will fail.");
    }

    return 0;
}

int nyx_wf_run(nyx_wf_ctx_t *ctx)
{
    if (!ctx || !ctx->workflow)
        return NYX_WF_ERR_PARAM;

    preflight_privilege_check(ctx);

    int has_errors = 0;

    for (size_t i = 0; i < ctx->exec_order_len; i++) {
        size_t step_idx = ctx->exec_order[i];
        int ret = execute_step(ctx, step_idx);
        if (ret != NYX_WF_SUCCESS) {
            nyx_log(NYX_LOG_ERROR, "Workflow aborted at step '%s'",
                    ctx->workflow->steps[step_idx].id);
            return ret;
        }
        if (ctx->status[step_idx] == NYX_WF_STATUS_ERROR)
            has_errors = 1;
    }

    return has_errors ? NYX_WF_ERR_EXEC : NYX_WF_SUCCESS;
}

nyx_json_t *nyx_wf_get_results(const nyx_wf_ctx_t *ctx)
{
    if (!ctx || !ctx->workflow)
        return nyx_json_object();

    nyx_json_t *steps = nyx_json_object();

    for (size_t i = 0; i < ctx->workflow->step_count; i++) {
        const char *id = ctx->workflow->steps[i].id;
        if (!id)
            continue;

        nyx_json_t *step_out = nyx_json_object();

        const char *status_str;
        switch (ctx->status[i]) {
        case NYX_WF_STATUS_DONE:
            status_str = "success";
            break;
        case NYX_WF_STATUS_SKIPPED:
            status_str = "skipped";
            break;
        case NYX_WF_STATUS_ERROR:
            status_str = "error";
            break;
        default:
            status_str = "pending";
            break;
        }
        nyx_json_set(step_out, "status", nyx_json_string(status_str));

        if (ctx->results[i]) {
            const nyx_json_t *results = nyx_json_get(ctx->results[i], "results");
            if (results) {
                char *s = nyx_json_serialize(results, 0);
                nyx_json_t *clone = s ? nyx_json_parse(s) : nyx_json_object();
                free(s);
                nyx_json_set(step_out, "results", clone);
            }

            const nyx_json_t *iters = nyx_json_get(ctx->results[i], "iterations");
            if (iters) {
                char *s = nyx_json_serialize(iters, 0);
                nyx_json_t *clone = s ? nyx_json_parse(s) : nyx_json_array();
                free(s);
                nyx_json_set(step_out, "iterations", clone);
            }

            const nyx_json_t *error = nyx_json_get(ctx->results[i], "error");
            if (error) {
                char *s = nyx_json_serialize(error, 0);
                nyx_json_t *clone = s ? nyx_json_parse(s) : nyx_json_null();
                free(s);
                nyx_json_set(step_out, "error", clone);
            }
        }

        nyx_json_set(steps, id, step_out);
    }

    return steps;
}

void nyx_wf_ctx_free(nyx_wf_ctx_t *ctx)
{
    if (!ctx)
        return;

    if (ctx->results) {
        for (size_t i = 0; i < ctx->workflow->step_count; i++)
            nyx_json_free(ctx->results[i]);
        free(ctx->results);
    }

    free(ctx->status);
    free(ctx->session_id);
    free(ctx->bin_dir);
    free(ctx->exec_order);
    free(ctx);
}
