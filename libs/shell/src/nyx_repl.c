/**
 * @file nyx_repl.c
 * @brief Shared interactive REPL framework implementation
 * @author Neur0sis (2025)
 *
 * Uses linenoise (BSD-2-Clause, by Salvatore Sanfilippo) for line editing,
 * history recall via arrow keys, and tab completion.
 */

#define _GNU_SOURCE
#include "nyx_repl.h"
#include "nyx_tool_registry.h"
#include "linenoise.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- File-scope pointer for linenoise callbacks (no user-data arg) ---- */
static nyx_repl_t *g_active_repl;

/* ---- Internal types ---- */

struct nyx_repl {
    char *name;
    char *context;
    char *welcome;
    char *history_path;
    void *userdata;
    int exit_requested;

    nyx_repl_cmd_t *cmds;
    size_t cmd_count;
    size_t cmd_cap;
    nyx_repl_handler_fn fallback;
};

/* ---- Helpers ---- */

static char *safe_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d)
        memcpy(d, s, len + 1);
    return d;
}

static int mkdir_p(const char *path, mode_t mode)
{
    char tmp[512];
    size_t len = strlen(path);
    if (len >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(tmp, mode) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    return (mkdir(tmp, mode) != 0 && errno != EEXIST) ? -1 : 0;
}

static char *history_path_for(const char *name)
{
    const char *home = getenv("HOME");
    if (!home)
        home = "/tmp";

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.nyx", home);
    (void)mkdir_p(dir, 0700);

    size_t need = strlen(dir) + 1 + strlen(name) + strlen("_history") + 1;
    char *path = malloc(need);
    if (!path)
        return NULL;
    snprintf(path, need, "%s/%s_history", dir, name);
    return path;
}

/* ---- Tab completion callback ---- */

static void completion_callback(const char *buf, linenoiseCompletions *lc)
{
    const nyx_repl_t *repl = g_active_repl;
    if (!repl)
        return;

    size_t buflen = strlen(buf);

    const char *space = strchr(buf, ' ');

    if (!space) {
        /* First token: complete command names */
        for (size_t i = 0; i < repl->cmd_count; i++) {
            const char *name = repl->cmds[i].name;
            if (name && strncmp(name, buf, buflen) == 0)
                linenoiseAddCompletion(lc, name);
        }
        static const char *builtins[] = {"help", "history", "exit", "quit", NULL};
        for (int i = 0; builtins[i]; i++) {
            if (strncmp(builtins[i], buf, buflen) == 0)
                linenoiseAddCompletion(lc, builtins[i]);
        }
        if (repl->context && strncmp("back", buf, buflen) == 0)
            linenoiseAddCompletion(lc, "back");

        if (repl->fallback) {
            size_t tc = nyx_tool_registry_count();
            for (size_t i = 0; i < tc; i++) {
                const nyx_tool_entry_t *t = nyx_tool_registry_at(i);
                if (t && t->name && strncmp(t->name, buf, buflen) == 0)
                    linenoiseAddCompletion(lc, t->name);
            }
        }
        return;
    }

    /* After first token: complete flags or values */
    const char *last_tok = buf;
    for (const char *p = buf; *p; p++) {
        if (*p == ' ')
            last_tok = p + 1;
    }

    size_t last_len = strlen(last_tok);

    /* Extract the command name (first word) */
    size_t cmd_len = (size_t)(space - buf);
    char cmd_name[64];
    if (cmd_len >= sizeof(cmd_name))
        return;
    memcpy(cmd_name, buf, cmd_len);
    cmd_name[cmd_len] = '\0';

    /* Find the command */
    const nyx_repl_cmd_t *cmd = NULL;
    for (size_t i = 0; i < repl->cmd_count; i++) {
        if (repl->cmds[i].name && strcmp(repl->cmds[i].name, cmd_name) == 0) {
            cmd = &repl->cmds[i];
            break;
        }
    }
    if (!cmd)
        return;

    /* Complete flags from the command's flag metadata */
    if (cmd->flags && cmd->flag_count > 0 && last_tok[0] == '-') {
        for (size_t i = 0; i < cmd->flag_count; i++) {
            const nyx_repl_flag_t *f = &cmd->flags[i];
            if (!f->name)
                continue;

            char full_line[512];
            size_t prefix_len = (size_t)(last_tok - buf);

            if (f->name[0] == '-' && strncmp(f->name, last_tok, last_len) == 0) {
                snprintf(full_line, sizeof(full_line), "%.*s%s", (int)prefix_len, buf, f->name);
                linenoiseAddCompletion(lc, full_line);
            }
        }
        return;
    }

    /* Value completion: check what flag preceded the current token */
    if (cmd->flags && cmd->flag_count > 0 && last_tok[0] != '-') {
        const char *prev_flag = NULL;
        int tokc = 0;
        char **tokv = nyx_repl_tokenize(buf, &tokc);
        if (tokv && tokc >= 2) {
            const char *prev = tokv[tokc - 1];
            if (prev[0] != '-' && tokc >= 3)
                prev = tokv[tokc - 2];
            if (prev[0] == '-')
                prev_flag = prev;
        }

        if (prev_flag) {
            nyx_compl_type_t ctype = NYX_COMPL_NONE;
            for (size_t i = 0; i < cmd->flag_count; i++) {
                if (cmd->flags[i].name && strcmp(cmd->flags[i].name, prev_flag) == 0) {
                    ctype = cmd->flags[i].compl_type;
                    break;
                }
            }

            if (ctype == NYX_COMPL_IFACE) {
                struct if_nameindex *ifs = if_nameindex();
                if (ifs) {
                    for (struct if_nameindex *p = ifs; p->if_name; p++) {
                        if (strncmp(p->if_name, last_tok, last_len) != 0)
                            continue;
                        char full_line[512];
                        size_t prefix_len = (size_t)(last_tok - buf);
                        snprintf(full_line, sizeof(full_line), "%.*s%s", (int)prefix_len, buf,
                                 p->if_name);
                        linenoiseAddCompletion(lc, full_line);
                    }
                    if_freenameindex(ifs);
                }
            } else if (ctype == NYX_COMPL_FILE) {
                /* Basic filesystem path completion */
                const char *partial = last_tok;
                char dir_path[512] = ".";
                const char *base = partial;
                const char *slash = strrchr(partial, '/');
                if (slash) {
                    size_t dlen = (size_t)(slash - partial);
                    if (dlen > 0 && dlen < sizeof(dir_path)) {
                        memcpy(dir_path, partial, dlen);
                        dir_path[dlen] = '\0';
                    }
                    base = slash + 1;
                }
                size_t base_len = strlen(base);
                DIR *d = opendir(dir_path);
                if (d) {
                    struct dirent *ent;
                    while ((ent = readdir(d)) != NULL) {
                        if (ent->d_name[0] == '.' && base[0] != '.')
                            continue;
                        if (strncmp(ent->d_name, base, base_len) != 0)
                            continue;
                        char full_line[1024];
                        size_t prefix_len = (size_t)(last_tok - buf);
                        if (slash) {
                            snprintf(full_line, sizeof(full_line), "%.*s%.*s/%s", (int)prefix_len,
                                     buf, (int)(slash - partial + 1), partial, ent->d_name);
                        } else {
                            snprintf(full_line, sizeof(full_line), "%.*s%s", (int)prefix_len, buf,
                                     ent->d_name);
                        }
                        linenoiseAddCompletion(lc, full_line);
                    }
                    closedir(d);
                }
            }
        }

        nyx_repl_free_tokens(tokv, tokc);
    }
}

/* ---- Tokenizer (public) ---- */

char **nyx_repl_tokenize(const char *line, int *argc_out)
{
    size_t argc = 0;
    size_t cap = 8;
    char **argv = calloc(cap + 1, sizeof(char *));
    char token[1024];
    size_t tlen = 0;
    int in_single = 0, in_double = 0, escape = 0;

    if (!argv) {
        *argc_out = 0;
        return NULL;
    }

    for (const char *p = line;; p++) {
        char c = *p;

        if (escape) {
            if (tlen + 1 < sizeof(token))
                token[tlen++] = c;
            escape = 0;
        } else if (c == '\\' && !in_single) {
            escape = 1;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        } else if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if ((c == '\0' || isspace((unsigned char)c)) && !in_single && !in_double) {
            if (tlen > 0) {
                token[tlen] = '\0';
                if (argc == cap) {
                    size_t nc = cap * 2;
                    char **tmp = realloc(argv, (nc + 1) * sizeof(char *));
                    if (!tmp)
                        goto fail;
                    argv = tmp;
                    cap = nc;
                }
                argv[argc] = safe_strdup(token);
                if (!argv[argc])
                    goto fail;
                argc++;
                tlen = 0;
            }
            if (c == '\0')
                break;
        } else if (tlen + 1 < sizeof(token)) {
            token[tlen++] = c;
        }
    }

    argv[argc] = NULL;
    *argc_out = (int)argc;
    return argv;

fail:
    for (size_t i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
    *argc_out = 0;
    return NULL;
}

void nyx_repl_free_tokens(char **argv, int argc)
{
    if (!argv)
        return;
    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}

/* ---- REPL lifecycle ---- */

nyx_repl_t *nyx_repl_create(const char *name)
{
    nyx_repl_t *r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->name = safe_strdup(name ? name : "nyx");
    r->history_path = history_path_for(r->name);
    return r;
}

void nyx_repl_free(nyx_repl_t *repl)
{
    if (!repl)
        return;
    free(repl->name);
    free(repl->context);
    free(repl->welcome);
    free(repl->history_path);
    for (size_t i = 0; i < repl->cmd_count; i++) {
        free((char *)repl->cmds[i].name);
        free((char *)repl->cmds[i].usage);
        free((char *)repl->cmds[i].description);
        free((char *)repl->cmds[i].help);
    }
    free(repl->cmds);
    free(repl);
}

/* ---- Configuration ---- */

void nyx_repl_add_cmd(nyx_repl_t *repl, const nyx_repl_cmd_t *cmd)
{
    if (!repl || !cmd || !cmd->name)
        return;

    if (repl->cmd_count == repl->cmd_cap) {
        size_t nc = repl->cmd_cap ? repl->cmd_cap * 2 : 16;
        nyx_repl_cmd_t *tmp = realloc(repl->cmds, nc * sizeof(*tmp));
        if (!tmp)
            return;
        repl->cmds = tmp;
        repl->cmd_cap = nc;
    }

    nyx_repl_cmd_t *dst = &repl->cmds[repl->cmd_count++];
    dst->name = safe_strdup(cmd->name);
    dst->usage = safe_strdup(cmd->usage);
    dst->description = safe_strdup(cmd->description);
    dst->help = safe_strdup(cmd->help);
    dst->handler = cmd->handler;
    dst->flags = cmd->flags;
    dst->flag_count = cmd->flag_count;
}

void nyx_repl_add_cmds(nyx_repl_t *repl, const nyx_repl_cmd_t *cmds, size_t count)
{
    for (size_t i = 0; i < count; i++)
        nyx_repl_add_cmd(repl, &cmds[i]);
}

void nyx_repl_set_fallback(nyx_repl_t *repl, nyx_repl_handler_fn fn)
{
    if (repl)
        repl->fallback = fn;
}

void nyx_repl_set_userdata(nyx_repl_t *repl, void *data)
{
    if (repl)
        repl->userdata = data;
}

void *nyx_repl_get_userdata(const nyx_repl_t *repl)
{
    return repl ? repl->userdata : NULL;
}

void nyx_repl_set_context(nyx_repl_t *repl, const char *context)
{
    if (!repl)
        return;
    free(repl->context);
    repl->context = safe_strdup(context);
}

const char *nyx_repl_get_context(const nyx_repl_t *repl)
{
    return repl ? repl->context : NULL;
}

void nyx_repl_set_welcome(nyx_repl_t *repl, const char *message)
{
    if (!repl)
        return;
    free(repl->welcome);
    repl->welcome = safe_strdup(message);
}

void nyx_repl_request_exit(nyx_repl_t *repl)
{
    if (repl)
        repl->exit_requested = 1;
}

/* ---- Built-in commands ---- */

static void print_help(const nyx_repl_t *repl)
{
    printf("\nAvailable commands:\n\n");
    for (size_t i = 0; i < repl->cmd_count; i++) {
        const nyx_repl_cmd_t *c = &repl->cmds[i];
        printf("  %-14s %s\n", c->name, c->description ? c->description : "");
    }
    printf("\n");
    printf("  %-14s %s\n", "history", "Show command history");
    printf("  %-14s %s\n", "help", "Show this help (try 'help <command>')");
    if (repl->context)
        printf("  %-14s %s\n", "back", "Return to parent shell");
    printf("  %-14s %s\n", "exit", "Exit the shell");
    printf("\n");
}

static void print_cmd_help(const nyx_repl_t *repl, const char *cmd_name)
{
    for (size_t i = 0; i < repl->cmd_count; i++) {
        const nyx_repl_cmd_t *c = &repl->cmds[i];
        if (strcmp(c->name, cmd_name) != 0)
            continue;

        printf("\n");
        if (c->usage && c->usage[0])
            printf("  Usage: %s\n", c->usage);
        else
            printf("  Usage: %s\n", c->name);

        if (c->description && c->description[0])
            printf("         %s\n", c->description);

        if (c->help && c->help[0])
            printf("\n%s\n", c->help);
        else
            printf("\n  No detailed help available for '%s'.\n", c->name);

        printf("\n");
        return;
    }

    if (strcmp(cmd_name, "help") == 0 || strcmp(cmd_name, "exit") == 0 ||
        strcmp(cmd_name, "quit") == 0 || strcmp(cmd_name, "back") == 0 ||
        strcmp(cmd_name, "history") == 0) {
        printf("\n  '%s' is a built-in shell command.\n\n", cmd_name);
        return;
    }

    fprintf(stderr, "  Unknown command '%s'. Type 'help' to see all commands.\n", cmd_name);
}

/* ---- Main loop ---- */

int nyx_repl_run(nyx_repl_t *repl)
{
    if (!repl)
        return -1;
    repl->exit_requested = 0;

    /* Set up linenoise */
    linenoiseHistorySetMaxLen(500);
    if (repl->history_path)
        linenoiseHistoryLoad(repl->history_path);

    g_active_repl = repl;
    linenoiseSetCompletionCallback(completion_callback);

    if (repl->welcome)
        printf("%s\n", repl->welcome);

    while (!repl->exit_requested) {
        char prompt[128];
        if (repl->context)
            snprintf(prompt, sizeof(prompt), "%s:%s> ", repl->name, repl->context);
        else
            snprintf(prompt, sizeof(prompt), "%s> ", repl->name);

        char *raw = linenoise(prompt);
        if (!raw) {
            printf("\n");
            break;
        }

        /* Trim whitespace */
        char *line = raw;
        while (*line && isspace((unsigned char)*line))
            line++;
        {
            size_t len = strlen(line);
            while (len > 0 && isspace((unsigned char)line[len - 1]))
                line[--len] = '\0';
        }

        if (!line[0]) {
            linenoiseFree(raw);
            continue;
        }

        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            linenoiseFree(raw);
            break;
        }

        linenoiseHistoryAdd(line);
        if (repl->history_path)
            linenoiseHistorySave(repl->history_path);

        if (strcmp(line, "help") == 0) {
            print_help(repl);
            linenoiseFree(raw);
            continue;
        }

        if (strncmp(line, "help ", 5) == 0 && line[5]) {
            const char *arg = line + 5;
            while (*arg == ' ')
                arg++;
            if (*arg)
                print_cmd_help(repl, arg);
            linenoiseFree(raw);
            continue;
        }

        if (strcmp(line, "history") == 0) {
            printf("Use arrow up/down to recall previous commands.\n");
            linenoiseFree(raw);
            continue;
        }

        if (strcmp(line, "back") == 0 && repl->context) {
            linenoiseFree(raw);
            break;
        }

        if (strcmp(line, "clear") == 0) {
            linenoiseClearScreen();
            linenoiseFree(raw);
            continue;
        }

        int argc = 0;
        char **argv = nyx_repl_tokenize(line, &argc);
        if (!argv || argc == 0) {
            nyx_repl_free_tokens(argv, argc);
            linenoiseFree(raw);
            continue;
        }

        int handled = 0;
        for (size_t i = 0; i < repl->cmd_count; i++) {
            if (strcmp(argv[0], repl->cmds[i].name) == 0) {
                (void)repl->cmds[i].handler(argc, argv, repl->userdata);
                handled = 1;
                break;
            }
        }

        if (!handled && repl->fallback) {
            (void)repl->fallback(argc, argv, repl->userdata);
            handled = 1;
        }

        if (!handled) {
            fprintf(stderr, "%s: unknown command '%s'. Type 'help'.\n", repl->name, argv[0]);
        }

        nyx_repl_free_tokens(argv, argc);
        linenoiseFree(raw);
    }

    g_active_repl = NULL;
    return 0;
}
