/**
 * @file nyx_output.c
 * @brief Structured output envelope, session management, and CLI integration
 */

#include "nyx_output.h"
#include "nyx_error.h"
#include "nyx_logger.h"
#include "nyx_term.h"
#include "nyx_json.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define NYX_SESSION_DIR_BASE ".nyx/sessions"
#define NYX_SESSION_ID_LEN   8

struct nyx_output_ctx {
    char *tool;
    char *module;
    char *version;
    char *session_id;
    char *session_dir;
    char *capture_path;

    int json_stdout;

    nyx_json_t *config;
    nyx_json_t *results;
    char       *status;
    nyx_json_t *error;

    struct timeval start_time;
};

/* ====================================================================
 *  Internal helpers
 * ==================================================================== */

static char *safe_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d) memcpy(d, s, len + 1);
    return d;
}

static int mkdir_p(const char *path, mode_t mode)
{
    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

static int generate_hex_id(char *buf, size_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;

    unsigned char raw[16];
    size_t need = (len + 1) / 2;
    if (need > sizeof(raw)) need = sizeof(raw);

    ssize_t n = read(fd, raw, need);
    close(fd);
    if (n < (ssize_t)need) return -1;

    for (size_t i = 0; i < len / 2; i++)
        snprintf(buf + i * 2, 3, "%02x", raw[i]);
    buf[len] = '\0';
    return 0;
}

static char *get_session_base_dir(void)
{
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    size_t len = strlen(home) + 1 + strlen(NYX_SESSION_DIR_BASE) + 1;
    char *path = malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/%s", home, NYX_SESSION_DIR_BASE);
    return path;
}

static char *get_session_dir(const char *session_id)
{
    char *base = get_session_base_dir();
    if (!base) return NULL;

    size_t len = strlen(base) + 1 + strlen(session_id) + 1;
    char *path = malloc(len);
    if (!path) { free(base); return NULL; }
    snprintf(path, len, "%s/%s", base, session_id);
    free(base);
    return path;
}

static int ensure_session_dir(const char *dir)
{
    return mkdir_p(dir, 0700);
}

static const char *get_capture_env(void)
{
    const char *path = getenv("NYX_OUTPUT_CAPTURE_PATH");
    return (path && path[0]) ? path : NULL;
}

static char *get_iso_timestamp(void)
{
    time_t now = time(NULL);
    struct tm tm_info;
    gmtime_r(&now, &tm_info);
    char *buf = malloc(32);
    if (!buf) return NULL;
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    return buf;
}

static double elapsed_ms(const struct timeval *start)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    double s  = (double)(now.tv_sec  - start->tv_sec)  * 1000.0;
    double us = (double)(now.tv_usec - start->tv_usec) / 1000.0;
    return s + us;
}

static nyx_json_t *build_envelope(nyx_output_ctx_t *ctx)
{
    nyx_json_t *root = nyx_json_object();
    if (!root) return NULL;

    /* nyx metadata */
    nyx_json_t *nyx = nyx_json_object();
    nyx_json_set(nyx, "version", nyx_json_string(ctx->version ? ctx->version : "unknown"));
    nyx_json_set(nyx, "tool",    nyx_json_string(ctx->tool    ? ctx->tool    : "unknown"));
    nyx_json_set(nyx, "module",  nyx_json_string(ctx->module  ? ctx->module  : "unknown"));

    char *ts = get_iso_timestamp();
    nyx_json_set(nyx, "timestamp", nyx_json_string(ts ? ts : ""));
    free(ts);

    nyx_json_set(nyx, "duration_ms", nyx_json_real(elapsed_ms(&ctx->start_time)));

    if (ctx->session_id)
        nyx_json_set(nyx, "session_id", nyx_json_string(ctx->session_id));

    nyx_json_set(root, "nyx", nyx);

    /* config */
    if (ctx->config)
        nyx_json_set(root, "config", ctx->config);
    else
        nyx_json_set(root, "config", nyx_json_object());

    /* status */
    nyx_json_set(root, "status",
                 nyx_json_string(ctx->status ? ctx->status : "success"));

    /* error */
    if (ctx->error)
        nyx_json_set(root, "error", ctx->error);
    else
        nyx_json_set(root, "error", nyx_json_null());

    /* results */
    if (ctx->results)
        nyx_json_set(root, "results", ctx->results);
    else
        nyx_json_set(root, "results", nyx_json_object());

    /*
     * Ownership of config, results, error trees transferred into envelope.
     * NULL them out so nyx_output_free doesn't double-free.
     */
    ctx->config  = NULL;
    ctx->results = NULL;
    ctx->error   = NULL;

    return root;
}

/* ====================================================================
 *  Public API
 * ==================================================================== */

nyx_output_ctx_t *nyx_output_init(const char *tool, const char *module,
                                   const char *version)
{
    nyx_output_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->tool    = safe_strdup(tool);
    ctx->module  = safe_strdup(module);
    ctx->version = safe_strdup(version);
    ctx->status  = safe_strdup("success");
    gettimeofday(&ctx->start_time, NULL);

    return ctx;
}

nyx_json_t *nyx_output_build_envelope(nyx_output_ctx_t *ctx)
{
    if (!ctx) return NULL;

    if (ctx->status && strcmp(ctx->status, "success") == 0 && !ctx->error) {
        const nyx_error_context_t *err = nyx_error_get();
        if (err && err->code != 0)
            nyx_output_set_error_from_ctx(ctx);
    }

    return build_envelope(ctx);
}

int nyx_output_finish(nyx_output_ctx_t *ctx)
{
    if (!ctx) return -1;

    nyx_json_t *envelope = nyx_output_build_envelope(ctx);
    if (!envelope) return -1;

    int ret = 0;

    /* Write to stdout in JSON mode */
    if (ctx->json_stdout) {
        char *str = nyx_json_serialize(envelope, 2);
        if (str) {
            flockfile(stdout);
            fputs(str, stdout);
            fflush(stdout);
            funlockfile(stdout);
            free(str);
        } else {
            ret = -1;
        }
    }

    /* Write to orchestrator capture file */
    if (ctx->capture_path && ctx->capture_path[0]) {
        if (nyx_json_write_file(envelope, ctx->capture_path, 2) != 0) {
            nyx_log(NYX_LOG_WARN, "Failed to write capture file: %s", ctx->capture_path);
            ret = -1;
        } else {
            nyx_log(NYX_LOG_VERBOSE, "Capture output: %s", ctx->capture_path);
        }
    }

    /* Write to session directory */
    if (ctx->session_dir && ctx->tool) {
        /* Extract bare tool name (strip "nyx-" prefix) */
        const char *name = ctx->tool;
        if (strncmp(name, "nyx-", 4) == 0)
            name += 4;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s.nyx.json", ctx->session_dir, name);

        if (nyx_json_write_file(envelope, path, 2) != 0) {
            nyx_log(NYX_LOG_WARN, "Failed to write session file: %s", path);
            ret = -1;
        } else {
            nyx_log(NYX_LOG_VERBOSE, "Session output: %s", path);
        }
    }

    nyx_json_free(envelope);
    return ret;
}

void nyx_output_free(nyx_output_ctx_t *ctx)
{
    if (!ctx) return;
    free(ctx->tool);
    free(ctx->module);
    free(ctx->version);
    free(ctx->session_id);
    free(ctx->session_dir);
    free(ctx->capture_path);
    free(ctx->status);
    nyx_json_free(ctx->config);
    nyx_json_free(ctx->results);
    nyx_json_free(ctx->error);
    free(ctx);
}

/* ---- Session ---- */

int nyx_output_set_session(nyx_output_ctx_t *ctx, const char *session_id)
{
    if (!ctx || !session_id) return -1;

    free(ctx->session_id);
    ctx->session_id = safe_strdup(session_id);

    free(ctx->session_dir);
    ctx->session_dir = get_session_dir(session_id);
    if (!ctx->session_dir) return -1;

    return ensure_session_dir(ctx->session_dir);
}

int nyx_output_new_session(nyx_output_ctx_t *ctx)
{
    if (!ctx) return -1;

    char id[NYX_SESSION_ID_LEN + 1];
    if (generate_hex_id(id, NYX_SESSION_ID_LEN) != 0)
        return -1;

    return nyx_output_set_session(ctx, id);
}

const char *nyx_output_get_session_id(const nyx_output_ctx_t *ctx)
{
    return ctx ? ctx->session_id : NULL;
}

/* ---- Mode control ---- */

void nyx_output_set_json_stdout(nyx_output_ctx_t *ctx, int enabled)
{
    if (ctx) ctx->json_stdout = enabled ? 1 : 0;
}

int nyx_output_is_json_mode(const nyx_output_ctx_t *ctx)
{
    if (!ctx) return 0;
    return ctx->json_stdout;
}

int nyx_output_has_structured_sink(const nyx_output_ctx_t *ctx)
{
    if (!ctx) return 0;
    return ctx->json_stdout ||
           (ctx->session_dir && ctx->session_dir[0]);
}

void nyx_output_set_capture_path(nyx_output_ctx_t *ctx, const char *path)
{
    if (!ctx) return;
    free(ctx->capture_path);
    ctx->capture_path = safe_strdup(path && path[0] ? path : NULL);
}

/* ---- Envelope fields ---- */

void nyx_output_set_config(nyx_output_ctx_t *ctx, nyx_json_t *config)
{
    if (!ctx) { nyx_json_free(config); return; }
    nyx_json_free(ctx->config);
    ctx->config = config;
}

void nyx_output_set_results(nyx_output_ctx_t *ctx, nyx_json_t *results)
{
    if (!ctx) { nyx_json_free(results); return; }
    nyx_json_free(ctx->results);
    ctx->results = results;
}

void nyx_output_set_status(nyx_output_ctx_t *ctx, const char *status)
{
    if (!ctx) return;
    free(ctx->status);
    ctx->status = safe_strdup(status);
}

void nyx_output_set_error_from_ctx(nyx_output_ctx_t *ctx)
{
    if (!ctx) return;
    const nyx_error_context_t *err = nyx_error_get();
    if (!err || err->code == 0) return;

    nyx_json_t *e = nyx_json_object();
    nyx_json_set(e, "domain",  nyx_json_int(err->domain));
    nyx_json_set(e, "code",    nyx_json_int(err->code));

    const char *msg = err->message;
    nyx_json_set(e, "message", nyx_json_string(msg[0] ? msg : nyx_error_str(err->domain, err->code)));

    if (err->suggestion[0])
        nyx_json_set(e, "suggestion", nyx_json_string(err->suggestion));

    nyx_json_free(ctx->error);
    ctx->error = e;
    free(ctx->status);
    ctx->status = safe_strdup("error");
}

void nyx_output_emit_error(nyx_output_ctx_t *ctx)
{
    if (!ctx) return;
    nyx_output_set_error_from_ctx(ctx);
    if (!ctx->error) {
        free(ctx->status);
        ctx->status = safe_strdup("error");
    }
    nyx_output_finish(ctx);
}

void nyx_output_set_error_msg(nyx_output_ctx_t *ctx, const char *message)
{
    if (!ctx) return;

    nyx_json_t *e = nyx_json_object();
    nyx_json_set(e, "message", nyx_json_string(message ? message : "Unknown error"));
    nyx_json_free(ctx->error);
    ctx->error = e;

    free(ctx->status);
    ctx->status = safe_strdup("error");
}

void nyx_output_emit_error_msg(nyx_output_ctx_t *ctx, const char *message)
{
    if (!ctx) return;
    nyx_output_set_error_msg(ctx, message);
    nyx_output_finish(ctx);
}

int nyx_output_argv_has_json(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0)
            return 1;
    }
    return 0;
}

/* ---- Load previous output ---- */

nyx_json_t *nyx_output_load_results(const char *session_id,
                                     const char *tool_name)
{
    if (!session_id || !tool_name) return NULL;

    char *dir = get_session_dir(session_id);
    if (!dir) return NULL;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.nyx.json", dir, tool_name);
    free(dir);

    nyx_json_t *root = nyx_json_parse_file(path);
    if (!root) return NULL;

    /* Return just the "results" subtree */
    nyx_json_t *results = nyx_json_get(root, "results");
    if (!results) {
        nyx_json_free(root);
        return NULL;
    }

    /*
     * The results node is part of root's tree. We can't extract it
     * without detaching. Instead, return the full envelope -- the caller
     * can use nyx_json_get() on it.
     */
    return root;
}

/* ---- CLI helper ---- */

nyx_output_ctx_t *nyx_output_from_cli(const char *tool, const char *module,
                                       const char *version,
                                       int has_json, const char *session_id)
{
    nyx_output_ctx_t *ctx = nyx_output_init(tool, module, version);
    if (!ctx) return NULL;

    if (has_json) {
        nyx_output_set_json_stdout(ctx, 1);
        nyx_logger_verbose = -1;
        nyx_term_set_enabled(0);
    } else {
        nyx_term_set_enabled(1);
    }

    if (session_id && session_id[0]) {
        if (nyx_output_set_session(ctx, session_id) != 0) {
            nyx_log(NYX_LOG_WARN, "Failed to initialize session: %s", session_id);
        }
    }

    {
        const char *capture_path = get_capture_env();
        if (capture_path)
            nyx_output_set_capture_path(ctx, capture_path);
    }

    return ctx;
}
