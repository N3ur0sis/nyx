/**
 * @file nyx_main.c
 * @brief Entry point for the NYX master CLI / interactive shell
 * @author Neur0sis (2025)
 *
 * Launches the NYX interactive shell by default.  The shell provides
 * tool contexts, workflow execution, and session management -- all
 * backed by the shared REPL library and the global tool registry.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nyx_cmd.h"
#include "nyx_cli.h"
#include "nyx_error.h"
#include "nyx_logger.h"
#include "nyx_priv.h"
#include "nyx_repl.h"
#include "nyx_tool_registry.h"
#include "nyx_output.h"
#include "nyx_json.h"
#include "nyx_workflow.h"

/* ---- Forward declarations for command handlers ---- */
static int cmd_run(int argc, char **argv, void *data);
static int cmd_session(int argc, char **argv, void *data);
static int cmd_info(int argc, char **argv, void *data);
static int cmd_version(int argc, char **argv, void *data);
static int cmd_tools(int argc, char **argv, void *data);
static int shell_fallback(int argc, char **argv, void *data);

typedef struct {
    nyx_repl_t *repl;
} nyx_ctx_t;

/* ---- Flag metadata for tab-completion ---- */

static const nyx_repl_flag_t run_flags[] = {
    { "--var",     NYX_COMPL_NONE },
    { "-V",        NYX_COMPL_NONE },
    { "-J",        NYX_COMPL_NONE },
    { "--json",    NYX_COMPL_NONE },
    { "-S",        NYX_COMPL_NONE },
    { "--session", NYX_COMPL_NONE },
};

/* ---- Shell commands ---- */

static const nyx_repl_cmd_t master_commands[] = {
    {
        .name = "run",
        .usage = "run <workflow.json> [--var K=V ...]",
        .description = "Execute a workflow file",
        .help =
            "  Loads a JSON workflow and executes it through the workflow engine.\n"
            "  Each step invokes a registered tool in-process.\n"
            "\n"
            "  Arguments:\n"
            "    <workflow.json>    Path to the workflow definition file [required]\n"
            "    --var K=V          Override a workflow variable (repeatable)\n"
            "\n"
            "  Examples:\n"
            "    run workflows/net-discovery.json\n"
            "    run scan.json --var cidr=10.0.0.0/24\n",
        .handler = cmd_run,
        .flags = run_flags,
        .flag_count = sizeof(run_flags) / sizeof(run_flags[0])
    },
    {
        .name = "session",
        .usage = "session list|show <id>|clean",
        .description = "Manage sessions",
        .help =
            "  Subcommands:\n"
            "    session list           List all saved sessions\n"
            "    session show <id>      Show JSON results for a session\n"
            "    session clean          Remove all session data\n"
            "\n"
            "  Sessions are stored in ~/.nyx/sessions/.\n",
        .handler = cmd_session
    },
    {
        .name = "info",
        .usage = "info",
        .description = "Show framework info",
        .help = NULL,
        .handler = cmd_info
    },
    {
        .name = "tools",
        .usage = "tools",
        .description = "List registered tools",
        .help =
            "  Shows all tools registered in the current binary.\n"
            "  Type a tool name to enter its interactive context.\n"
            "  You can also run a tool command directly:\n"
            "    <tool> <command> [args...]\n"
            "\n"
            "  Example:\n"
            "    pingsweep scan -c 192.168.1.0/24\n",
        .handler = cmd_tools
    },
    {
        .name = "version",
        .usage = "version",
        .description = "Show framework version",
        .help = NULL,
        .handler = cmd_version
    },
};

/* ---- Tool context sub-shell ---- */

static void enter_tool_context(nyx_ctx_t *ctx, const char *tool_name)
{
    const nyx_tool_entry_t *tool = nyx_tool_registry_find(tool_name);
    if (!tool) {
        fprintf(stderr, "nyx: tool '%s' not found\n", tool_name);
        return;
    }

    printf("Entered tool context: %s (%s)\n", tool->name, tool->description);

    if (tool->required_priv != NYX_PRIV_NONE &&
        !nyx_priv_check((nyx_priv_t)tool->required_priv)) {
        nyx_log(NYX_LOG_WARN, "This tool requires %s. "
                "Some commands will fail without root.",
                nyx_priv_label((nyx_priv_t)tool->required_priv));
        nyx_log(NYX_LOG_WARN, "Restart with: sudo nyx");
    }

    nyx_repl_t *sub = nyx_repl_create("nyx");
    nyx_repl_set_context(sub, tool->name);

    if (tool->cmds && tool->cmd_count > 0) {
        nyx_repl_add_cmds(sub, (const nyx_repl_cmd_t *)tool->cmds,
                           tool->cmd_count);
    }

    nyx_repl_set_welcome(sub,
        "Type 'help' for commands, 'help <cmd>' for details, 'back' to return.");

    nyx_repl_run(sub);
    nyx_repl_free(sub);

    (void)ctx;
}

/* ---- Command implementations ---- */

static int cmd_run(int argc, char **argv, void *data)
{
    (void)data;
    if (argc < 2) {
        fprintf(stderr, "Usage: run <workflow.json> [--var K=V] [-J] [-S id]\n");
        return 1;
    }

    const char *wf_path = argv[1];
    int json_mode = 0;
    const char *session_id = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0)
            json_mode = 1;
        else if ((strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--session") == 0)
                 && i + 1 < argc)
            session_id = argv[++i];
    }

    nyx_output_ctx_t *out = nyx_output_from_cli(
        "nyx-run", "workflow", NYX_VERSION, json_mode, session_id);

    nyx_workflow_t *wf = nyx_wf_load_file(wf_path);
    if (!wf) {
        nyx_log(NYX_LOG_ERROR, "Failed to load workflow: %s", wf_path);
        if (json_mode) nyx_output_emit_error_msg(out, "Failed to load workflow");
        nyx_output_free(out);
        return 1;
    }

    /* Apply --var overrides */
    for (int i = 2; i < argc; i++) {
        if ((strcmp(argv[i], "--var") == 0 || strcmp(argv[i], "-V") == 0)
            && i + 1 < argc) {
            i++;
            char *eq = strchr(argv[i], '=');
            if (eq) {
                size_t klen = (size_t)(eq - argv[i]);
                char key[128];
                if (klen < sizeof(key)) {
                    memcpy(key, argv[i], klen);
                    key[klen] = '\0';
                    nyx_wf_set_var(wf, key, eq + 1);
                }
            }
        }
    }

    if (nyx_wf_validate(wf) != NYX_WF_SUCCESS) {
        nyx_log(NYX_LOG_ERROR, "Workflow validation failed");
        if (json_mode) nyx_output_emit_error_msg(out, "Workflow validation failed");
        nyx_wf_free(wf);
        nyx_output_free(out);
        return 1;
    }

    nyx_log(NYX_LOG_INFO, "Executing workflow '%s' (%zu steps)...",
            wf->id ? wf->id : wf_path, wf->step_count);

    const char *sess = nyx_output_get_session_id(out);
    nyx_wf_ctx_t *wf_ctx = nyx_wf_ctx_create(wf, sess, NULL);
    if (!wf_ctx) {
        nyx_log(NYX_LOG_ERROR, "Failed to create workflow context");
        nyx_wf_free(wf);
        nyx_output_free(out);
        return 1;
    }

    nyx_json_t *config = nyx_json_object();
    nyx_json_set(config, "workflow_file", nyx_json_string(wf_path));
    if (wf->id) nyx_json_set(config, "workflow_id", nyx_json_string(wf->id));
    nyx_json_set(config, "step_count", nyx_json_int((long)wf->step_count));
    nyx_output_set_config(out, config);

    int rc = nyx_wf_run(wf_ctx);

    nyx_json_t *wf_results = nyx_json_object();
    if (wf->id) nyx_json_set(wf_results, "id", nyx_json_string(wf->id));
    nyx_json_set(wf_results, "steps", nyx_wf_get_results(wf_ctx));
    nyx_output_set_results(out, wf_results);

    if (rc == NYX_WF_SUCCESS)
        nyx_output_set_status(out, "success");
    else
        nyx_output_set_error_msg(out, "Workflow completed with step errors.");
    nyx_output_finish(out);

    if (!json_mode) {
        printf("\n");
        if (rc == NYX_WF_SUCCESS)
            nyx_log(NYX_LOG_SUCCESS, "Workflow completed successfully.");
        else
            nyx_log(NYX_LOG_ERROR, "Workflow completed with errors.");

        printf("\n  Step Summary:\n");
        for (size_t i = 0; i < wf->step_count; i++) {
            const char *icon;
            switch (wf_ctx->status[i]) {
                case NYX_WF_STATUS_DONE:    icon = COLOR_GREEN "✓" COLOR_RESET; break;
                case NYX_WF_STATUS_SKIPPED: icon = COLOR_YELLOW "○" COLOR_RESET; break;
                case NYX_WF_STATUS_ERROR:   icon = COLOR_RED "✗" COLOR_RESET; break;
                default:                    icon = "?"; break;
            }
            printf("  %s %s (%s)\n", icon, wf->steps[i].id, wf->steps[i].tool);
        }
        printf("\n");
    }

    nyx_wf_ctx_free(wf_ctx);
    nyx_wf_free(wf);
    nyx_output_free(out);
    return (rc == NYX_WF_SUCCESS) ? 0 : 1;
}

static int cmd_session(int argc, char **argv, void *data)
{
    (void)data;
    return nyx_cmd_session(argc, argv);
}

static int cmd_info(int argc, char **argv, void *data)
{
    (void)argc; (void)argv; (void)data;

    printf(COLOR_CYAN "NYX Framework v%s" COLOR_RESET "\n\n", NYX_VERSION);
    printf("Registered tools:\n");
    size_t count = nyx_tool_registry_count();
    for (size_t i = 0; i < count; i++) {
        const nyx_tool_entry_t *t = nyx_tool_registry_at(i);
        printf("  %-16s  [%s v%s]  %s\n",
               t->name, t->module, t->version,
               t->description ? t->description : "");
    }
    if (count == 0)
        printf("  (none)\n");
    printf("\n");
    return 0;
}

static int cmd_version(int argc, char **argv, void *data)
{
    (void)argc; (void)argv; (void)data;
    printf("nyx %s\n", NYX_VERSION);
    return 0;
}

static int cmd_tools(int argc, char **argv, void *data)
{
    (void)argc; (void)argv; (void)data;

    printf(COLOR_CYAN "Available tools:" COLOR_RESET "\n\n");
    size_t count = nyx_tool_registry_count();
    for (size_t i = 0; i < count; i++) {
        const nyx_tool_entry_t *t = nyx_tool_registry_at(i);
        printf("  %-16s  %s\n", t->name,
               t->description ? t->description : "");
    }
    if (count == 0)
        printf("  (none)\n");
    printf("\nType a tool name to enter its context, or 'run' to execute a workflow.\n\n");
    return 0;
}

static int shell_fallback(int argc, char **argv, void *data)
{
    nyx_ctx_t *ctx = (nyx_ctx_t *)data;

    const nyx_tool_entry_t *tool = nyx_tool_registry_find(argv[0]);
    if (tool) {
        if (argc == 1) {
            enter_tool_context(ctx, argv[0]);
            return 0;
        }

        if (tool->cmds && tool->cmd_count > 0) {
            for (size_t i = 0; i < tool->cmd_count; i++) {
                const nyx_repl_cmd_t *c =
                    (const nyx_repl_cmd_t *)&tool->cmds[i];
                if (strcmp(argv[1], c->name) == 0)
                    return c->handler(argc - 1, argv + 1, NULL);
            }
            fprintf(stderr, "nyx: tool '%s' has no command '%s'. "
                    "Type '%s' to enter its shell.\n",
                    tool->name, argv[1], tool->name);
            return 1;
        }

        enter_tool_context(ctx, argv[0]);
        return 0;
    }

    fprintf(stderr, "nyx: unknown command '%s'. Type 'help' or 'tools'.\n",
            argv[0]);
    return 1;
}

/* ---- Entry point ---- */

static void print_banner(void)
{
    nyx_cli_banner_config_t banner = {
        .module = "framework",
        .tool_name = "nyx",
        .version = NYX_VERSION,
        .author = "Neur0sis",
        .primary_color = NYX_CLI_COLOR_CYAN,
        .secondary_color = NYX_CLI_COLOR_MAGENTA,
        .style = NYX_CLI_BANNER_ASCII_ART
    };
    nyx_cli_print_banner(&banner);
}

int main(int argc, char *argv[])
{
    nyx_tools_register_all();

    if (argc >= 2) {
        const char *cmd = argv[1];

        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 ||
            strcmp(cmd, "-h") == 0) {
            print_banner();
            printf(
                COLOR_CYAN "Usage:" COLOR_RESET " nyx [command]\n\n"
                "Run with no arguments to enter the interactive shell.\n\n"
                COLOR_CYAN "Commands:" COLOR_RESET "\n"
                "  run       Execute a workflow file\n"
                "  session   Manage sessions\n"
                "  info      Show registered tools and framework status\n"
                "  tools     List available tools\n"
                "  version   Show NYX framework version\n"
                "  help      Show this help message\n\n"
            );
            return 0;
        }

        if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0 ||
            strcmp(cmd, "-v") == 0) {
            printf("nyx %s\n", NYX_VERSION);
            return 0;
        }

        if (strcmp(cmd, "run") == 0)
            return cmd_run(argc - 1, argv + 1, NULL);
        if (strcmp(cmd, "session") == 0)
            return nyx_cmd_session(argc - 1, argv + 1);
        if (strcmp(cmd, "info") == 0)
            return cmd_info(0, NULL, NULL);
        if (strcmp(cmd, "tools") == 0)
            return cmd_tools(0, NULL, NULL);

        fprintf(stderr, "nyx: unknown command '%s'. Run 'nyx help'.\n", cmd);
        return 1;
    }

    if (!isatty(STDIN_FILENO)) {
        print_banner();
        printf("nyx: interactive shell requires a terminal.\n");
        return 0;
    }

    print_banner();

    nyx_ctx_t ctx = {0};
    nyx_repl_t *repl = nyx_repl_create("nyx");
    ctx.repl = repl;

    nyx_repl_add_cmds(repl, master_commands,
                       sizeof(master_commands) / sizeof(master_commands[0]));
    nyx_repl_set_fallback(repl, shell_fallback);
    nyx_repl_set_userdata(repl, &ctx);

    char welcome[256];
    snprintf(welcome, sizeof(welcome),
             "Type 'help' for commands, 'tools' to list tools, "
             "or a tool name to enter its context.");
    nyx_repl_set_welcome(repl, welcome);

    int rc = nyx_repl_run(repl);
    nyx_repl_free(repl);
    nyx_tool_registry_cleanup();
    return rc;
}
