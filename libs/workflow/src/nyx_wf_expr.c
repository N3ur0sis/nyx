/**
 * @file nyx_wf_expr.c
 * @brief Expression evaluator for the NYX workflow engine
 * @author Neur0sis (2025)
 *
 * Implements a recursive descent evaluator for the ${...} expression
 * language. Operates on nyx_json_t trees, producing new nodes as output.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nyx_wf_expr.h"
#include "nyx_logger.h"

/* ====================================================================
 *  Helpers
 * ==================================================================== */

static char *str_dup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d)
        memcpy(d, s, len + 1);
    return d;
}

static void skip_ws(const char **p)
{
    while (**p && isspace((unsigned char)**p))
        (*p)++;
}

static int is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

/**
 * Deep-clone a JSON node (the nyx_json API doesn't expose this, so we
 * reimplement a minimal version).
 */
static nyx_json_t *json_clone(const nyx_json_t *src)
{
    if (!src)
        return nyx_json_null();
    switch (nyx_json_type(src)) {
    case NYX_JSON_NULL:
        return nyx_json_null();
    case NYX_JSON_BOOL:
        return nyx_json_bool(nyx_json_get_bool(src));
    case NYX_JSON_INT:
        return nyx_json_int(nyx_json_get_int(src));
    case NYX_JSON_DOUBLE:
        return nyx_json_real(nyx_json_get_real(src));
    case NYX_JSON_STRING:
        return nyx_json_string(nyx_json_get_string(src));
    case NYX_JSON_ARRAY: {
        nyx_json_t *arr = nyx_json_array();
        size_t len = nyx_json_length(src);
        for (size_t i = 0; i < len; i++)
            nyx_json_append(arr, json_clone(nyx_json_at(src, i)));
        return arr;
    }
    case NYX_JSON_OBJECT: {
        nyx_json_t *obj = nyx_json_object();
        /* Walk children via nyx_json_at + length approach is not
           key-aware; serialize and re-parse as a workaround. */
        char *s = nyx_json_serialize(src, 0);
        if (s) {
            nyx_json_t *parsed = nyx_json_parse(s);
            free(s);
            if (parsed) {
                nyx_json_free(obj);
                return parsed;
            }
        }
        return obj;
    }
    }
    return nyx_json_null();
}

/**
 * Convert a JSON value to its string representation for CLI argument building.
 */
static char *json_to_string(const nyx_json_t *node)
{
    if (!node)
        return str_dup("");
    switch (nyx_json_type(node)) {
    case NYX_JSON_STRING:
        return str_dup(nyx_json_get_string(node));
    case NYX_JSON_INT: {
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", nyx_json_get_int(node));
        return str_dup(buf);
    }
    case NYX_JSON_DOUBLE: {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g", nyx_json_get_real(node));
        return str_dup(buf);
    }
    case NYX_JSON_BOOL:
        return str_dup(nyx_json_get_bool(node) ? "true" : "false");
    case NYX_JSON_NULL:
        return str_dup("null");
    default: {
        char *s = nyx_json_serialize(node, 0);
        return s ? s : str_dup("");
    }
    }
}

/* ====================================================================
 *  Path resolution: walk a JSON tree by dot-separated keys and indices
 * ==================================================================== */

/**
 * Resolve a dot-separated path like "results.hosts" against a JSON node.
 * Returns a pointer into the existing tree (does NOT clone).
 */
static const nyx_json_t *resolve_path(const nyx_json_t *root, const char *path)
{
    if (!root || !path || !*path)
        return root;

    const nyx_json_t *cur = root;
    const char *p = path;

    while (*p && cur) {
        /* Skip leading dot */
        if (*p == '.')
            p++;
        if (!*p)
            break;

        /* Check for array index [N] */
        if (*p == '[') {
            p++;
            if (*p == '*') {
                /* [*] is handled at the pipe level, not here */
                return cur;
            }
            char *end;
            long idx = strtol(p, &end, 10);
            if (*end != ']')
                return NULL;
            p = end + 1;
            if (nyx_json_type(cur) != NYX_JSON_ARRAY)
                return NULL;
            cur = nyx_json_at(cur, (size_t)idx);
            continue;
        }

        /* Extract key segment */
        const char *start = p;
        while (*p && *p != '.' && *p != '[' && *p != '|' && *p != ' ' && *p != '>' && *p != '<' &&
               *p != '=' && *p != '!')
            p++;

        size_t klen = (size_t)(p - start);
        if (klen == 0)
            break;

        char key[128];
        if (klen >= sizeof(key))
            return NULL;
        memcpy(key, start, klen);
        key[klen] = '\0';

        if (nyx_json_type(cur) != NYX_JSON_OBJECT)
            return NULL;
        cur = nyx_json_get(cur, key);
    }

    return cur;
}

/* ====================================================================
 *  Pipe operations: filter, select, count, first, flat
 * ==================================================================== */

static int compare_values(const nyx_json_t *a, const char *op, const char *val_str)
{
    if (!a)
        return 0;

    if (strcmp(val_str, "true") == 0 || strcmp(val_str, "false") == 0) {
        int bval = (strcmp(val_str, "true") == 0) ? 1 : 0;
        if (nyx_json_type(a) != NYX_JSON_BOOL)
            return 0;
        int aval = nyx_json_get_bool(a);
        if (strcmp(op, "==") == 0)
            return aval == bval;
        if (strcmp(op, "!=") == 0)
            return aval != bval;
        return 0;
    }

    if (nyx_json_type(a) == NYX_JSON_STRING) {
        const char *s = nyx_json_get_string(a);
        /* Strip quotes from val_str if present */
        char clean[256];
        size_t vlen = strlen(val_str);
        if (vlen >= 2 && val_str[0] == '"' && val_str[vlen - 1] == '"') {
            memcpy(clean, val_str + 1, vlen - 2);
            clean[vlen - 2] = '\0';
        } else {
            snprintf(clean, sizeof(clean), "%s", val_str);
        }
        int cmp = strcmp(s, clean);
        if (strcmp(op, "==") == 0)
            return cmp == 0;
        if (strcmp(op, "!=") == 0)
            return cmp != 0;
        return 0;
    }

    /* Numeric comparison */
    double av = 0, bv = 0;
    if (nyx_json_type(a) == NYX_JSON_INT)
        av = (double)nyx_json_get_int(a);
    else if (nyx_json_type(a) == NYX_JSON_DOUBLE)
        av = nyx_json_get_real(a);
    else
        return 0;

    char *end;
    bv = strtod(val_str, &end);
    if (*end != '\0' && !isspace((unsigned char)*end))
        return 0;

    if (strcmp(op, "==") == 0)
        return av == bv;
    if (strcmp(op, "!=") == 0)
        return av != bv;
    if (strcmp(op, ">") == 0)
        return av > bv;
    if (strcmp(op, "<") == 0)
        return av < bv;
    if (strcmp(op, ">=") == 0)
        return av >= bv;
    if (strcmp(op, "<=") == 0)
        return av <= bv;
    return 0;
}

static nyx_json_t *pipe_filter(const nyx_json_t *arr, const char **p)
{
    skip_ws(p);
    /* Parse: filter <key> <op> <value> */
    const char *start = *p;
    while (**p && is_ident_char(**p))
        (*p)++;
    size_t klen = (size_t)(*p - start);
    if (klen == 0)
        return NULL;

    char key[128];
    if (klen >= sizeof(key))
        return NULL;
    memcpy(key, start, klen);
    key[klen] = '\0';

    skip_ws(p);

    /* Parse operator */
    char op[4] = {0};
    size_t oi = 0;
    while (**p && (**p == '=' || **p == '!' || **p == '>' || **p == '<') && oi < 3) {
        op[oi++] = **p;
        (*p)++;
    }
    if (oi == 0)
        return NULL;

    skip_ws(p);

    /* Parse value */
    start = *p;
    if (**p == '"') {
        (*p)++;
        while (**p && **p != '"')
            (*p)++;
        if (**p == '"')
            (*p)++;
        /* include quotes */
    } else {
        while (**p && !isspace((unsigned char)**p) && **p != '|' && **p != '}')
            (*p)++;
    }
    size_t vlen = (size_t)(*p - start);
    char val[256];
    if (vlen >= sizeof(val))
        return NULL;
    memcpy(val, start, vlen);
    val[vlen] = '\0';

    if (nyx_json_type(arr) != NYX_JSON_ARRAY)
        return NULL;

    nyx_json_t *result = nyx_json_array();
    size_t len = nyx_json_length(arr);
    for (size_t i = 0; i < len; i++) {
        const nyx_json_t *elem = nyx_json_at(arr, i);
        if (nyx_json_type(elem) != NYX_JSON_OBJECT)
            continue;
        const nyx_json_t *field = nyx_json_get(elem, key);
        if (compare_values(field, op, val))
            nyx_json_append(result, json_clone(elem));
    }
    return result;
}

static nyx_json_t *pipe_select(const nyx_json_t *arr, const char **p)
{
    skip_ws(p);
    const char *start = *p;
    while (**p && is_ident_char(**p))
        (*p)++;
    size_t klen = (size_t)(*p - start);
    if (klen == 0)
        return NULL;

    char key[128];
    if (klen >= sizeof(key))
        return NULL;
    memcpy(key, start, klen);
    key[klen] = '\0';

    if (nyx_json_type(arr) != NYX_JSON_ARRAY)
        return NULL;

    nyx_json_t *result = nyx_json_array();
    size_t len = nyx_json_length(arr);
    for (size_t i = 0; i < len; i++) {
        const nyx_json_t *elem = nyx_json_at(arr, i);
        const nyx_json_t *field = NULL;
        if (nyx_json_type(elem) == NYX_JSON_OBJECT)
            field = nyx_json_get(elem, key);
        nyx_json_append(result, field ? json_clone(field) : nyx_json_null());
    }
    return result;
}

static nyx_json_t *pipe_count(const nyx_json_t *arr)
{
    if (nyx_json_type(arr) != NYX_JSON_ARRAY)
        return nyx_json_int(0);
    return nyx_json_int((long)nyx_json_length(arr));
}

static nyx_json_t *pipe_first(const nyx_json_t *arr)
{
    if (nyx_json_type(arr) != NYX_JSON_ARRAY || nyx_json_length(arr) == 0)
        return nyx_json_null();
    return json_clone(nyx_json_at(arr, 0));
}

static nyx_json_t *pipe_flat(const nyx_json_t *arr)
{
    if (nyx_json_type(arr) != NYX_JSON_ARRAY)
        return json_clone(arr);

    nyx_json_t *result = nyx_json_array();
    size_t len = nyx_json_length(arr);
    for (size_t i = 0; i < len; i++) {
        const nyx_json_t *elem = nyx_json_at(arr, i);
        if (nyx_json_type(elem) == NYX_JSON_ARRAY) {
            size_t inner = nyx_json_length(elem);
            for (size_t j = 0; j < inner; j++)
                nyx_json_append(result, json_clone(nyx_json_at(elem, j)));
        } else {
            nyx_json_append(result, json_clone(elem));
        }
    }
    return result;
}

/**
 * Apply pipe operations to a JSON value.
 * @param val  Current value (consumed -- may be freed; caller uses return)
 * @param p    Pointer into the expression after '|'; advanced past the pipe ops
 * @return New JSON value with pipes applied
 */
static nyx_json_t *apply_pipes(nyx_json_t *val, const char **p)
{
    nyx_json_t *cur = val;

    while (1) {
        skip_ws(p);
        if (**p != '|')
            break;
        (*p)++;
        skip_ws(p);

        const char *op_start = *p;
        while (**p && isalpha((unsigned char)**p))
            (*p)++;
        size_t oplen = (size_t)(*p - op_start);

        nyx_json_t *next = NULL;

        if (oplen == 6 && strncmp(op_start, "filter", 6) == 0) {
            skip_ws(p);
            next = pipe_filter(cur, p);
        } else if (oplen == 6 && strncmp(op_start, "select", 6) == 0) {
            skip_ws(p);
            next = pipe_select(cur, p);
        } else if (oplen == 5 && strncmp(op_start, "count", 5) == 0) {
            next = pipe_count(cur);
        } else if (oplen == 5 && strncmp(op_start, "first", 5) == 0) {
            next = pipe_first(cur);
        } else if (oplen == 4 && strncmp(op_start, "flat", 4) == 0) {
            next = pipe_flat(cur);
        } else {
            char unknown_op[64];
            size_t copy = oplen < sizeof(unknown_op) - 1 ? oplen : sizeof(unknown_op) - 1;
            memcpy(unknown_op, op_start, copy);
            unknown_op[copy] = '\0';
            nyx_log(NYX_LOG_WARN, "Unknown pipe operation: '%s'", unknown_op);
            break;
        }

        if (!next) {
            nyx_log(NYX_LOG_WARN, "Pipe operation failed, returning null");
        }

        if (cur != val)
            nyx_json_free(cur);
        cur = next ? next : nyx_json_null();
    }

    return cur;
}

/* ====================================================================
 *  Core expression evaluation
 * ==================================================================== */

/**
 * Strips ${ } delimiters if present, returning a pointer into the inner text.
 * Sets *end to the closing brace position (or end of string).
 */
static const char *strip_delimiters(const char *expr, const char **end_out)
{
    const char *p = expr;
    skip_ws(&p);
    if (p[0] == '$' && p[1] == '{') {
        p += 2;
        const char *close = strchr(p, '}');
        if (close)
            *end_out = close;
        else
            *end_out = p + strlen(p);
    } else {
        *end_out = p + strlen(p);
    }
    return p;
}

/**
 * Resolve the root of an expression to a JSON node.
 * Root can be: a step ID, "vars", "each", or "env".
 */
static const nyx_json_t *resolve_root(const char *root_id, const nyx_wf_ctx_t *ctx,
                                      const nyx_json_t *each_item)
{
    if (!root_id || !ctx)
        return NULL;

    if (strcmp(root_id, "vars") == 0)
        return ctx->workflow ? ctx->workflow->vars : NULL;

    if (strcmp(root_id, "each") == 0)
        return each_item;

    /* Look up step by ID */
    if (ctx->workflow) {
        for (size_t i = 0; i < ctx->workflow->step_count; i++) {
            if (strcmp(ctx->workflow->steps[i].id, root_id) == 0)
                return ctx->results ? ctx->results[i] : NULL;
        }
    }

    nyx_log(NYX_LOG_WARN, "Expression root '%s' not found (not a step ID or builtin)", root_id);
    return NULL;
}

nyx_json_t *nyx_wf_expr_eval(const char *expr, const nyx_wf_ctx_t *ctx, const nyx_json_t *each_item)
{
    if (!expr)
        return nyx_json_null();

    const char *end;
    const char *p = strip_delimiters(expr, &end);
    skip_ws(&p);

    /* Extract root identifier */
    const char *root_start = p;
    while (p < end && is_ident_char(*p))
        p++;
    size_t rlen = (size_t)(p - root_start);
    if (rlen == 0)
        return nyx_json_null();

    char root_id[128];
    if (rlen >= sizeof(root_id))
        return nyx_json_null();
    memcpy(root_id, root_start, rlen);
    root_id[rlen] = '\0';

    const nyx_json_t *root = resolve_root(root_id, ctx, each_item);
    if (!root)
        return nyx_json_null();

    /* Skip dot after root if present */
    if (*p == '.')
        p++;

    /* Collect the remaining path (up to pipe or comparator or end) */
    const char *path_start = p;
    while (p < end && *p != '|' && *p != '>' && *p != '<' && !(*p == '=' && *(p + 1) == '=') &&
           !(*p == '!' && *(p + 1) == '='))
        p++;

    /* Trim trailing whitespace from path */
    const char *path_end = p;
    while (path_end > path_start && isspace((unsigned char)*(path_end - 1)))
        path_end--;

    char path[512];
    size_t plen = (size_t)(path_end - path_start);
    if (plen >= sizeof(path))
        plen = sizeof(path) - 1;
    memcpy(path, path_start, plen);
    path[plen] = '\0';

    const nyx_json_t *resolved = resolve_path(root, path);
    if (!resolved)
        return nyx_json_null();

    nyx_json_t *result = json_clone(resolved);

    /* Apply pipe operations if any */
    if (*p == '|') {
        nyx_json_t *piped = apply_pipes(result, &p);
        if (piped != result) {
            nyx_json_free(result);
            result = piped;
        }
    }

    return result;
}

int nyx_wf_expr_eval_bool(const char *expr, const nyx_wf_ctx_t *ctx, const nyx_json_t *each_item)
{
    if (!expr)
        return 1;

    const char *end;
    const char *p = strip_delimiters(expr, &end);
    skip_ws(&p);

    /* Extract root */
    const char *root_start = p;
    while (p < end && is_ident_char(*p))
        p++;
    size_t rlen = (size_t)(p - root_start);
    if (rlen == 0)
        return 0;

    char root_id[128];
    if (rlen >= sizeof(root_id))
        return 0;
    memcpy(root_id, root_start, rlen);
    root_id[rlen] = '\0';

    const nyx_json_t *root = resolve_root(root_id, ctx, each_item);
    if (!root)
        return 0;

    if (*p == '.')
        p++;

    /* Collect path up to comparator */
    const char *path_start = p;
    while (p < end && *p != '>' && *p != '<' && !(*p == '=' && *(p + 1) == '=') &&
           !(*p == '!' && *(p + 1) == '=') && *p != '|')
        p++;

    const char *path_end = p;
    while (path_end > path_start && isspace((unsigned char)*(path_end - 1)))
        path_end--;

    char path[512];
    size_t plen = (size_t)(path_end - path_start);
    if (plen >= sizeof(path))
        plen = sizeof(path) - 1;
    memcpy(path, path_start, plen);
    path[plen] = '\0';

    const nyx_json_t *resolved = resolve_path(root, path);
    if (!resolved)
        return 0;

    skip_ws(&p);

    /* No comparator: treat as truthy check */
    if (p >= end || (!*p) || *p == '}') {
        switch (nyx_json_type(resolved)) {
        case NYX_JSON_BOOL:
            return nyx_json_get_bool(resolved);
        case NYX_JSON_INT:
            return nyx_json_get_int(resolved) != 0;
        case NYX_JSON_DOUBLE:
            return nyx_json_get_real(resolved) != 0.0;
        case NYX_JSON_STRING:
            return nyx_json_get_string(resolved)[0] != '\0';
        case NYX_JSON_NULL:
            return 0;
        case NYX_JSON_ARRAY:
            return nyx_json_length(resolved) > 0;
        case NYX_JSON_OBJECT:
            return 1;
        }
        return 0;
    }

    /* Parse comparator */
    char op[4] = {0};
    size_t oi = 0;
    while (p < end && (*p == '>' || *p == '<' || *p == '=' || *p == '!') && oi < 3) {
        op[oi++] = *p;
        p++;
    }

    skip_ws(&p);

    /* Parse value */
    const char *val_start = p;
    while (p < end && *p != '}' && !isspace((unsigned char)*p))
        p++;
    char val[256];
    size_t vlen = (size_t)(p - val_start);
    if (vlen >= sizeof(val))
        vlen = sizeof(val) - 1;
    memcpy(val, val_start, vlen);
    val[vlen] = '\0';

    return compare_values(resolved, op, val);
}

char *nyx_wf_expr_resolve_string(const char *str, const nyx_wf_ctx_t *ctx,
                                 const nyx_json_t *each_item)
{
    if (!str)
        return NULL;

    /* Fast path: no expressions */
    if (!strchr(str, '$'))
        return str_dup(str);

    /* If the entire string is a single ${...}, return evaluated value */
    const char *p = str;
    skip_ws(&p);
    if (p[0] == '$' && p[1] == '{') {
        const char *close = strchr(p + 2, '}');
        if (close) {
            const char *after = close + 1;
            while (*after && isspace((unsigned char)*after))
                after++;
            if (!*after) {
                nyx_json_t *val = nyx_wf_expr_eval(str, ctx, each_item);
                char *result = json_to_string(val);
                nyx_json_free(val);
                return result;
            }
        }
    }

    /* Mixed string: replace each ${...} occurrence */
    size_t cap = strlen(str) * 2 + 64;
    char *out = malloc(cap);
    if (!out)
        return NULL;
    size_t len = 0;
    out[0] = '\0';

    p = str;
    while (*p) {
        if (p[0] == '$' && p[1] == '{') {
            const char *close = strchr(p + 2, '}');
            if (!close) {
                if (len + 2 >= cap) {
                    cap *= 2;
                    char *tmp = realloc(out, cap);
                    if (!tmp) {
                        free(out);
                        return NULL;
                    }
                    out = tmp;
                }
                out[len++] = *p++;
                continue;
            }

            size_t expr_len = (size_t)(close - p + 1);
            char *expr_str = malloc(expr_len + 1);
            if (!expr_str) {
                free(out);
                return NULL;
            }
            memcpy(expr_str, p, expr_len);
            expr_str[expr_len] = '\0';

            nyx_json_t *val = nyx_wf_expr_eval(expr_str, ctx, each_item);
            free(expr_str);

            char *val_str = json_to_string(val);
            nyx_json_free(val);

            if (val_str) {
                size_t vlen = strlen(val_str);
                while (len + vlen + 1 >= cap) {
                    cap *= 2;
                    char *tmp = realloc(out, cap);
                    if (!tmp) {
                        free(val_str);
                        free(out);
                        return NULL;
                    }
                    out = tmp;
                }
                memcpy(out + len, val_str, vlen);
                len += vlen;
                free(val_str);
            }

            p = close + 1;
        } else {
            if (len + 2 >= cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) {
                    free(out);
                    return NULL;
                }
                out = tmp;
            }
            out[len++] = *p++;
        }
    }

    out[len] = '\0';
    return out;
}

/* ====================================================================
 *  Reference scanning (for dependency graph construction)
 * ==================================================================== */

static int is_builtin_root(const char *id)
{
    return strcmp(id, "vars") == 0 || strcmp(id, "each") == 0 || strcmp(id, "env") == 0;
}

int nyx_wf_expr_scan_refs(const char *str, char ***ids, size_t *count)
{
    if (!str || !ids || !count)
        return -1;

    *ids = NULL;
    *count = 0;

    size_t cap = 4;
    char **arr = malloc(cap * sizeof(char *));
    if (!arr)
        return -1;
    size_t n = 0;

    const char *p = str;
    while (*p) {
        if (p[0] == '$' && p[1] == '{') {
            p += 2;
            while (*p && isspace((unsigned char)*p))
                p++;

            const char *start = p;
            while (*p && is_ident_char(*p))
                p++;
            size_t klen = (size_t)(p - start);
            if (klen == 0)
                continue;

            char id[128];
            if (klen >= sizeof(id))
                continue;
            memcpy(id, start, klen);
            id[klen] = '\0';

            if (is_builtin_root(id))
                continue;

            /* Deduplicate */
            int found = 0;
            for (size_t i = 0; i < n; i++) {
                if (strcmp(arr[i], id) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (n >= cap) {
                    cap *= 2;
                    char **tmp = realloc(arr, cap * sizeof(char *));
                    if (!tmp) {
                        goto fail;
                    }
                    arr = tmp;
                }
                arr[n] = str_dup(id);
                if (!arr[n])
                    goto fail;
                n++;
            }
        } else {
            p++;
        }
    }

    *ids = arr;
    *count = n;
    return 0;

fail:
    for (size_t i = 0; i < n; i++)
        free(arr[i]);
    free(arr);
    return -1;
}
