/**
 * @file nyx_wf_parser.c
 * @brief Workflow JSON file parser
 * @author Neur0sis (2025)
 *
 * Parses a JSON workflow file into a nyx_workflow_t structure,
 * extracts steps, and scans expressions to build dependency lists.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nyx_wf_types.h"
#include "nyx_wf_expr.h"
#include "nyx_json.h"
#include "nyx_logger.h"

/* ====================================================================
 *  Helpers
 * ==================================================================== */

static char *str_dup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

static char *json_string_dup(const nyx_json_t *obj, const char *key)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    if (!v || nyx_json_type(v) != NYX_JSON_STRING) return NULL;
    return str_dup(nyx_json_get_string(v));
}

/* ====================================================================
 *  Scan all expressions in a step to discover step references
 * ==================================================================== */

/**
 * Collect step ID refs from a single string and merge into the step's
 * dependency list, resolving IDs to indices.
 */
static void collect_refs_from_string(const char *str,
                                      nyx_wf_step_t *step,
                                      const nyx_wf_step_t *all_steps,
                                      size_t step_count)
{
    if (!str) return;

    char **ids = NULL;
    size_t id_count = 0;
    if (nyx_wf_expr_scan_refs(str, &ids, &id_count) != 0) return;

    for (size_t i = 0; i < id_count; i++) {
        /* Find the index of this step ID */
        size_t idx = step_count; /* sentinel */
        for (size_t j = 0; j < step_count; j++) {
            if (all_steps[j].id && strcmp(all_steps[j].id, ids[i]) == 0) {
                idx = j;
                break;
            }
        }
        if (idx == step_count) { free(ids[i]); continue; }

        /* Deduplicate in deps */
        int dup = 0;
        for (size_t d = 0; d < step->dep_count; d++) {
            if (step->deps[d] == idx) { dup = 1; break; }
        }
        if (!dup) {
            size_t *tmp = realloc(step->deps,
                                  (step->dep_count + 1) * sizeof(size_t));
            if (tmp) {
                step->deps = tmp;
                step->deps[step->dep_count++] = idx;
            }
        }
        free(ids[i]);
    }
    free(ids);
}

/**
 * Scan a JSON subtree for strings containing ${} expressions.
 * Since nyx_json_at() only works on arrays, we serialize the node
 * and scan the resulting string for ${} patterns.
 */
static void scan_json_for_refs(const nyx_json_t *node,
                                nyx_wf_step_t *step,
                                const nyx_wf_step_t *all_steps,
                                size_t step_count)
{
    if (!node) return;

    char *s = nyx_json_serialize(node, 0);
    if (!s) return;

    collect_refs_from_string(s, step, all_steps, step_count);
    free(s);
}

/* ====================================================================
 *  Public parser
 * ==================================================================== */

nyx_workflow_t *nyx_wf_parse_json(const nyx_json_t *root)
{
    if (!root || nyx_json_type(root) != NYX_JSON_OBJECT) return NULL;

    nyx_workflow_t *wf = calloc(1, sizeof(*wf));
    if (!wf) return NULL;

    /* Parse workflow metadata */
    const nyx_json_t *wf_meta = nyx_json_get(root, "workflow");
    if (wf_meta && nyx_json_type(wf_meta) == NYX_JSON_OBJECT) {
        wf->id = json_string_dup(wf_meta, "id");
        wf->name = json_string_dup(wf_meta, "name");
        wf->version = json_string_dup(wf_meta, "version");
        wf->description = json_string_dup(wf_meta, "description");
    }

    /* Parse vars (clone the JSON tree) */
    const nyx_json_t *vars = nyx_json_get(root, "vars");
    if (vars && nyx_json_type(vars) == NYX_JSON_OBJECT) {
        char *s = nyx_json_serialize(vars, 0);
        if (s) {
            wf->vars = nyx_json_parse(s);
            free(s);
        }
    }
    if (!wf->vars) wf->vars = nyx_json_object();

    /* Parse steps */
    const nyx_json_t *steps = nyx_json_get(root, "steps");
    if (!steps || nyx_json_type(steps) != NYX_JSON_ARRAY) {
        nyx_log(NYX_LOG_ERROR, "Workflow missing 'steps' array");
        nyx_json_free(wf->vars);
        free(wf->id); free(wf->name); free(wf->version); free(wf->description);
        free(wf);
        return NULL;
    }

    size_t sc = nyx_json_length(steps);
    if (sc == 0) {
        nyx_log(NYX_LOG_ERROR, "Workflow has no steps");
        nyx_json_free(wf->vars);
        free(wf->id); free(wf->name); free(wf->version); free(wf->description);
        free(wf);
        return NULL;
    }

    wf->steps = calloc(sc, sizeof(nyx_wf_step_t));
    if (!wf->steps) {
        nyx_json_free(wf->vars);
        free(wf->id); free(wf->name); free(wf->version); free(wf->description);
        free(wf);
        return NULL;
    }
    wf->step_count = sc;

    for (size_t i = 0; i < sc; i++) {
        const nyx_json_t *step_json = nyx_json_at(steps, i);
        if (!step_json || nyx_json_type(step_json) != NYX_JSON_OBJECT) continue;

        nyx_wf_step_t *step = &wf->steps[i];
        step->id = json_string_dup(step_json, "id");
        step->tool = json_string_dup(step_json, "tool");

        /* Clone params */
        const nyx_json_t *params = nyx_json_get(step_json, "params");
        if (params) {
            char *s = nyx_json_serialize(params, 0);
            if (s) {
                step->params = nyx_json_parse(s);
                free(s);
            }
        }
        if (!step->params) step->params = nyx_json_object();

        /* for_each expression */
        const nyx_json_t *fe = nyx_json_get(step_json, "for_each");
        if (fe && nyx_json_type(fe) == NYX_JSON_STRING)
            step->for_each = str_dup(nyx_json_get_string(fe));

        /* when expression */
        const nyx_json_t *when = nyx_json_get(step_json, "when");
        if (when && nyx_json_type(when) == NYX_JSON_STRING)
            step->when = str_dup(nyx_json_get_string(when));

        /* GUI metadata (clone) */
        const nyx_json_t *meta = nyx_json_get(step_json, "meta");
        if (meta) {
            char *s = nyx_json_serialize(meta, 0);
            if (s) { step->meta = nyx_json_parse(s); free(s); }
        }
    }

    /* Build dependency lists by scanning expressions */
    for (size_t i = 0; i < sc; i++) {
        nyx_wf_step_t *step = &wf->steps[i];
        scan_json_for_refs(step->params, step, wf->steps, sc);
        collect_refs_from_string(step->for_each, step, wf->steps, sc);
        collect_refs_from_string(step->when, step, wf->steps, sc);
    }

    return wf;
}

nyx_workflow_t *nyx_wf_load_file(const char *path)
{
    if (!path) return NULL;

    nyx_json_t *root = nyx_json_parse_file(path);
    if (!root) {
        nyx_log(NYX_LOG_ERROR, "Failed to parse workflow file: %s", path);
        return NULL;
    }

    nyx_workflow_t *wf = nyx_wf_parse_json(root);
    nyx_json_free(root);
    return wf;
}

void nyx_wf_free(nyx_workflow_t *wf)
{
    if (!wf) return;

    for (size_t i = 0; i < wf->step_count; i++) {
        nyx_wf_step_t *s = &wf->steps[i];
        free(s->id);
        free(s->tool);
        nyx_json_free(s->params);
        free(s->for_each);
        free(s->when);
        nyx_json_free(s->meta);
        free(s->deps);
    }
    free(wf->steps);

    nyx_json_free(wf->vars);
    free(wf->id);
    free(wf->name);
    free(wf->version);
    free(wf->description);
    free(wf);
}
