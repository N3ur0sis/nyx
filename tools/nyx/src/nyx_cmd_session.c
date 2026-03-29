/**
 * @file nyx_cmd_session.c
 * @brief "nyx session" subcommand -- list, show, and clean sessions
 * @author Neur0sis (2025)
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "nyx_cmd.h"
#include "nyx_json.h"
#include "nyx_logger.h"

#define SESSION_DIR_BASE ".nyx/sessions"

static char *get_sessions_dir(void)
{
    const char *home = getenv("HOME");
    if (!home)
        home = "/tmp";

    size_t len = strlen(home) + 1 + strlen(SESSION_DIR_BASE) + 1;
    char *path = malloc(len);
    if (!path)
        return NULL;
    snprintf(path, len, "%s/%s", home, SESSION_DIR_BASE);
    return path;
}

/* ---- list ---- */

static int cmd_list(void)
{
    char *base = get_sessions_dir();
    if (!base)
        return 1;

    DIR *d = opendir(base);
    if (!d) {
        printf("No sessions found.\n");
        free(base);
        return 0;
    }

    printf(COLOR_CYAN "%-14s  %-22s  %-10s  %s" COLOR_RESET "\n", "SESSION", "DATE", "STATUS",
           "TOOLS");
    printf("%-14s  %-22s  %-10s  %s\n", "──────────────", "──────────────────────", "──────────",
           "─────────────");

    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        char sess_path[512];
        snprintf(sess_path, sizeof(sess_path), "%s/%s", base, ent->d_name);

        struct stat st;
        if (stat(sess_path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        /* Scan session directory for *.nyx.json files */
        DIR *sd = opendir(sess_path);
        if (!sd)
            continue;

        char tools_buf[256] = "";
        size_t tlen = 0;
        char date_str[32] = "unknown";
        char status_str[32] = "unknown";
        int file_count = 0;

        struct dirent *fent;
        while ((fent = readdir(sd)) != NULL) {
            size_t nlen = strlen(fent->d_name);
            if (nlen < 10)
                continue;
            if (strcmp(fent->d_name + nlen - 9, ".nyx.json") != 0)
                continue;

            /* Extract tool name from filename (strip .nyx.json) */
            char tool_name[64];
            size_t tnlen = nlen - 9;
            if (tnlen >= sizeof(tool_name))
                tnlen = sizeof(tool_name) - 1;
            memcpy(tool_name, fent->d_name, tnlen);
            tool_name[tnlen] = '\0';

            if (file_count > 0 && tlen + 2 < sizeof(tools_buf)) {
                tools_buf[tlen++] = ',';
                tools_buf[tlen++] = ' ';
            }
            size_t add = strlen(tool_name);
            if (tlen + add < sizeof(tools_buf)) {
                memcpy(tools_buf + tlen, tool_name, add);
                tlen += add;
                tools_buf[tlen] = '\0';
            }

            /* Read the first file for date and status */
            if (file_count == 0) {
                char fpath[600];
                snprintf(fpath, sizeof(fpath), "%s/%s", sess_path, fent->d_name);
                nyx_json_t *root = nyx_json_parse_file(fpath);
                if (root) {
                    const nyx_json_t *nyx_meta = nyx_json_get(root, "nyx");
                    if (nyx_meta) {
                        const nyx_json_t *ts = nyx_json_get(nyx_meta, "timestamp");
                        if (ts && nyx_json_type(ts) == NYX_JSON_STRING) {
                            snprintf(date_str, sizeof(date_str), "%s", nyx_json_get_string(ts));
                        }
                    }
                    const nyx_json_t *st_node = nyx_json_get(root, "status");
                    if (st_node && nyx_json_type(st_node) == NYX_JSON_STRING)
                        snprintf(status_str, sizeof(status_str), "%s",
                                 nyx_json_get_string(st_node));
                    nyx_json_free(root);
                }
            }
            file_count++;
        }
        closedir(sd);

        /* Color the status */
        const char *sc = "";
        const char *sr = "";
        if (strcmp(status_str, "success") == 0) {
            sc = COLOR_GREEN;
            sr = COLOR_RESET;
        } else if (strcmp(status_str, "error") == 0) {
            sc = COLOR_RED;
            sr = COLOR_RESET;
        }

        printf("%-14s  %-22s  %s%-10s%s  %s\n", ent->d_name, date_str, sc, status_str, sr,
               tools_buf);
        count++;
    }
    closedir(d);
    free(base);

    if (count == 0)
        printf("No sessions found.\n");
    else
        printf("\n%d session(s)\n", count);

    return 0;
}

/* ---- show ---- */

static int cmd_show(const char *session_id, int json_mode)
{
    char *base = get_sessions_dir();
    if (!base)
        return 1;

    char sess_path[512];
    snprintf(sess_path, sizeof(sess_path), "%s/%s", base, session_id);
    free(base);

    DIR *d = opendir(sess_path);
    if (!d) {
        fprintf(stderr, "nyx: session '%s' not found\n", session_id);
        return 1;
    }

    nyx_json_t *merged = nyx_json_object();
    nyx_json_set(merged, "session_id", nyx_json_string(session_id));
    nyx_json_t *tools_obj = nyx_json_object();

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        size_t nlen = strlen(ent->d_name);
        if (nlen < 10)
            continue;
        if (strcmp(ent->d_name + nlen - 9, ".nyx.json") != 0)
            continue;

        char tool_name[64];
        size_t tnlen = nlen - 9;
        if (tnlen >= sizeof(tool_name))
            tnlen = sizeof(tool_name) - 1;
        memcpy(tool_name, ent->d_name, tnlen);
        tool_name[tnlen] = '\0';

        char fpath[600];
        snprintf(fpath, sizeof(fpath), "%s/%s", sess_path, ent->d_name);

        nyx_json_t *root = nyx_json_parse_file(fpath);
        if (!root)
            continue;

        if (json_mode) {
            /* Clone into merged object */
            char *s = nyx_json_serialize(root, 0);
            nyx_json_t *clone = s ? nyx_json_parse(s) : nyx_json_object();
            free(s);
            nyx_json_set(tools_obj, tool_name, clone);
        } else {
            /* Human-readable output */
            const nyx_json_t *nyx_meta = nyx_json_get(root, "nyx");
            const nyx_json_t *status = nyx_json_get(root, "status");

            printf(COLOR_CYAN "── %s " COLOR_RESET, tool_name);

            if (status && nyx_json_type(status) == NYX_JSON_STRING) {
                const char *sv = nyx_json_get_string(status);
                if (strcmp(sv, "success") == 0)
                    printf(COLOR_GREEN "(%s)" COLOR_RESET, sv);
                else if (strcmp(sv, "error") == 0)
                    printf(COLOR_RED "(%s)" COLOR_RESET, sv);
                else
                    printf("(%s)", sv);
            }

            if (nyx_meta) {
                const nyx_json_t *ts = nyx_json_get(nyx_meta, "timestamp");
                if (ts && nyx_json_type(ts) == NYX_JSON_STRING)
                    printf("  %s", nyx_json_get_string(ts));
                const nyx_json_t *dur = nyx_json_get(nyx_meta, "duration_ms");
                if (dur && nyx_json_type(dur) == NYX_JSON_DOUBLE)
                    printf("  (%.1f ms)", nyx_json_get_real(dur));
            }
            printf("\n");

            const nyx_json_t *results = nyx_json_get(root, "results");
            if (results) {
                char *pretty = nyx_json_serialize(results, 2);
                if (pretty) {
                    printf("%s\n", pretty);
                    free(pretty);
                }
            }
            printf("\n");
        }

        nyx_json_free(root);
    }
    closedir(d);

    if (json_mode) {
        nyx_json_set(merged, "tools", tools_obj);
        char *out = nyx_json_serialize(merged, 2);
        if (out) {
            printf("%s\n", out);
            free(out);
        }
    }

    nyx_json_free(merged);
    return 0;
}

/* ---- clean ---- */

static int remove_dir_recursive(const char *path)
{
    DIR *d = opendir(path);
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char child[600];
        snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);

        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode))
            remove_dir_recursive(child);
        else
            unlink(child);
    }
    closedir(d);
    return rmdir(path);
}

static int cmd_clean(int argc, char **argv)
{
    int force = 0;
    int older_than_days = -1;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0)
            force = 1;
        else if (strcmp(argv[i], "--older-than") == 0 && i + 1 < argc) {
            older_than_days = atoi(argv[++i]);
        }
    }

    char *base = get_sessions_dir();
    if (!base)
        return 1;

    DIR *d = opendir(base);
    if (!d) {
        printf("No sessions to clean.\n");
        free(base);
        return 0;
    }

    time_t now = time(NULL);
    int removed = 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        char sess_path[512];
        snprintf(sess_path, sizeof(sess_path), "%s/%s", base, ent->d_name);

        struct stat st;
        if (stat(sess_path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        if (older_than_days >= 0) {
            double age_days = difftime(now, st.st_mtime) / 86400.0;
            if (age_days < (double)older_than_days)
                continue;
        }

        if (!force) {
            printf("Remove session %s? [y/N] ", ent->d_name);
            fflush(stdout);
            int c = getchar();
            /* Consume rest of line */
            while (c != '\n' && c != EOF)
                c = getchar();
            if (c != 'y' && c != 'Y')
                continue;
        }

        if (remove_dir_recursive(sess_path) == 0) {
            printf("  Removed %s\n", ent->d_name);
            removed++;
        } else {
            fprintf(stderr, "  Failed to remove %s: %s\n", ent->d_name, strerror(errno));
        }
    }
    closedir(d);
    free(base);

    printf("%d session(s) removed.\n", removed);
    return 0;
}

/* ---- dispatch ---- */

int nyx_cmd_session(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: nyx session <subcommand>\n"
                        "\n"
                        "Subcommands:\n"
                        "  list                   List all sessions\n"
                        "  show <id> [-J]         Show session details\n"
                        "  clean [--older-than N] Remove sessions\n");
        return 1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "list") == 0) {
        return cmd_list();
    }

    if (strcmp(sub, "show") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: nyx session show <session-id> [-J]\n");
            return 1;
        }
        int json = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0)
                json = 1;
        }
        return cmd_show(argv[2], json);
    }

    if (strcmp(sub, "clean") == 0) {
        return cmd_clean(argc - 2, argv + 2);
    }

    if (strcmp(sub, "--help") == 0 || strcmp(sub, "-h") == 0) {
        printf("Usage: nyx session <subcommand>\n"
               "\n"
               "Subcommands:\n"
               "  list                        List all sessions\n"
               "  show <id> [-J]              Show session details (-J for JSON)\n"
               "  clean [--older-than DAYS]   Remove sessions (--force to skip prompt)\n");
        return 0;
    }

    fprintf(stderr, "nyx session: unknown subcommand '%s'\n", sub);
    return 1;
}
