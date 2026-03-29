/**
 * @file nyx_run_cli.c
 * @brief CLI for the NYX workflow runner
 * @author Neur0sis (2025)
 *
 * Executes a JSON workflow file by loading, validating, and running
 * all steps in topological order. Produces a workflow-level output
 * envelope compatible with the NYX structured output system.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

#include "nyx_logger.h"
#include "nyx_cli.h"
#include "nyx_output.h"
#include "nyx_json.h"
#include "nyx_workflow.h"
#include "nyx_tool_registry.h"
#include "nyx_version.h"

#define TOOL_NAME "run"
#define NYX_MODULE "workflow"
#define MAX_VARS 64

static const nyx_cli_opt_def_t cli_options[] = {
    {'V', "var",     "KEY=VAL", "Override a workflow variable (repeatable)",
     NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_OPTIONAL},
    {'b', "bin-dir", "PATH",    "Path to NYX tool binaries",
     NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_OPTIONAL},
    {'J', "json",    NULL,      "Output results as JSON to stdout",
     NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
    {'S', "session", "ID",      "Use specific session ID",
     NYX_CLI_ARG_REQUIRED, NYX_CLI_FLAG_OPTIONAL},
    {'d', "debug",   NULL,      "Enable verbose execution logging",
     NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
    {'v', "version", NULL,      "Display version information",
     NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
    {'h', "help",    NULL,      "Show help information",
     NYX_CLI_ARG_NONE, NYX_CLI_FLAG_OPTIONAL},
};

static const size_t cli_option_count = sizeof(cli_options) / sizeof(cli_options[0]);

/**
 * Detect the bin directory from argv[0] (same directory as nyx-run).
 */
static char *detect_bin_dir(const char *argv0)
{
    char resolved[PATH_MAX];
    if (!argv0) return NULL;

    /* Try realpath first */
    if (realpath(argv0, resolved)) {
        char *dir = dirname(resolved);
        if (dir) return strdup(dir);
    }

    /* Fallback: dirname of argv[0] */
    char *copy = strdup(argv0);
    if (!copy) return NULL;
    char *dir = dirname(copy);
    char *result = strdup(dir);
    free(copy);
    return result;
}

/**
 * Parse --var KEY=VALUE pairs from CLI extra args and the standard option.
 * Since nyx_cli doesn't support repeated options natively, we also parse
 * extra_args that look like --var KEY=VAL.
 */
static void apply_var_overrides(nyx_workflow_t *wf,
                                 int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--var") == 0 || strcmp(argv[i], "-V") == 0)
            && i + 1 < argc) {
            i++;
            char *eq = strchr(argv[i], '=');
            if (!eq) {
                nyx_log(NYX_LOG_WARN, "Invalid --var format: %s (expected KEY=VALUE)", argv[i]);
                continue;
            }
            size_t klen = (size_t)(eq - argv[i]);
            char key[128];
            if (klen >= sizeof(key)) continue;
            memcpy(key, argv[i], klen);
            key[klen] = '\0';
            nyx_wf_set_var(wf, key, eq + 1);
        }
    }
}

int main(int argc, char *argv[])
{
    nyx_tools_register_all();

    nyx_cli_result_t *cli = nyx_cli_parse(argc, argv, cli_options, cli_option_count);
    if (!cli) {
        if (nyx_output_argv_has_json(argc, argv))
            printf("{\"status\":\"error\",\"error\":{\"message\":\"CLI parse failure\"}}\n");
        else
            fprintf(stderr, "nyx-run: CLI parse failure\n");
        return 1;
    }

    if (cli->error) {
        if (nyx_output_argv_has_json(argc, argv))
            printf("{\"status\":\"error\",\"error\":{\"message\":\"%s\"}}\n", cli->error_msg);
        else
            fprintf(stderr, "nyx-run: %s\n", cli->error_msg);
        nyx_cli_free_result(cli);
        return 1;
    }

    if (nyx_cli_has_option(cli, 'h')) {
        nyx_cli_print_usage("nyx-run <workflow.json> [OPTIONS]",
                            cli_options, cli_option_count,
                            "NYX Workflow Engine -- execute DAG-based workflow files");
        nyx_cli_free_result(cli);
        return 0;
    }

    if (nyx_cli_has_option(cli, 'v')) {
        printf("nyx-run %s\n", NYX_VERSION);
        nyx_cli_free_result(cli);
        return 0;
    }

    int json_mode = nyx_cli_has_option(cli, 'J');
    const char *session_id = nyx_cli_get_option(cli, 'S');
    const char *bin_dir_opt = nyx_cli_get_option(cli, 'b');
    int debug = nyx_cli_has_option(cli, 'd');
    int banner_enabled = !json_mode && getenv("NYX_NO_BANNER") == NULL;

    /* Set up structured output */
    nyx_output_ctx_t *out = nyx_output_from_cli(
        "nyx-run", NYX_MODULE, NYX_VERSION, json_mode, session_id);

    if (debug && !json_mode)
        nyx_set_verbose(1);

    /* Banner */
    if (banner_enabled) {
        nyx_cli_banner_config_t banner = {
            .module = NYX_MODULE,
            .tool_name = "nyx-run",
            .version = NYX_VERSION,
            .author = "Neur0sis",
            .primary_color = NYX_CLI_COLOR_CYAN,
            .secondary_color = NYX_CLI_COLOR_MAGENTA,
            .style = NYX_CLI_BANNER_ASCII_ART
        };
        nyx_cli_print_banner(&banner);
    }

    /* The workflow file must be provided as a positional argument */
    const char *wf_path = NULL;
    if (cli->extra_arg_count > 0)
        wf_path = cli->extra_args[0];

    if (!wf_path || !wf_path[0]) {
        if (nyx_output_is_json_mode(out)) {
            nyx_output_emit_error_msg(out, "No workflow file specified. Usage: nyx-run <workflow.json>");
            out = NULL;
        } else {
            nyx_log(NYX_LOG_ERROR, "No workflow file specified. Usage: nyx-run <workflow.json>");
        }
        nyx_output_free(out);
        nyx_cli_free_result(cli);
        return 1;
    }

    nyx_log(NYX_LOG_INFO, "Loading workflow: %s", wf_path);

    /* Load and parse */
    nyx_workflow_t *wf = nyx_wf_load_file(wf_path);
    if (!wf) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to load workflow file: %s", wf_path);
        if (nyx_output_is_json_mode(out)) {
            nyx_output_emit_error_msg(out, msg);
            out = NULL;
        } else {
            nyx_log(NYX_LOG_ERROR, "%s", msg);
        }
        nyx_output_free(out);
        nyx_cli_free_result(cli);
        return 1;
    }

    /* Apply --var overrides */
    apply_var_overrides(wf, argc, argv);

    /* Validate */
    nyx_log(NYX_LOG_INFO, "Validating workflow '%s' (%zu steps)...",
            wf->id ? wf->id : wf_path, wf->step_count);

    int rc = nyx_wf_validate(wf);
    if (rc != NYX_WF_SUCCESS) {
        const char *msg = (rc == NYX_WF_ERR_CYCLE)
                              ? "Workflow contains a dependency cycle"
                              : "Workflow validation failed";
        if (nyx_output_is_json_mode(out)) {
            nyx_output_emit_error_msg(out, msg);
            out = NULL;
        } else {
            nyx_log(NYX_LOG_ERROR, "%s", msg);
        }
        nyx_wf_free(wf);
        nyx_output_free(out);
        nyx_cli_free_result(cli);
        return 1;
    }

    nyx_log(NYX_LOG_SUCCESS, "Workflow valid. Execution order:");

    /* Detect bin directory */
    char *bin_dir = NULL;
    if (bin_dir_opt && bin_dir_opt[0])
        bin_dir = strdup(bin_dir_opt);
    else
        bin_dir = detect_bin_dir(argv[0]);

    /* Display execution order */
    {
        size_t *order = NULL;
        size_t order_len = 0;
        if (nyx_wf_topo_sort(wf, &order, &order_len) == NYX_WF_SUCCESS) {
            for (size_t i = 0; i < order_len; i++)
                nyx_log(NYX_LOG_INFO, "  %zu. %s (%s)",
                        i + 1, wf->steps[order[i]].id,
                        wf->steps[order[i]].tool);
            free(order);
        }
    }

    /* Create execution context */
    const char *sess = nyx_output_get_session_id(out);
    nyx_wf_ctx_t *ctx = nyx_wf_ctx_create(wf, sess, bin_dir);
    free(bin_dir);

    if (!ctx) {
        if (nyx_output_is_json_mode(out)) {
            nyx_output_emit_error_msg(out, "Failed to create workflow execution context");
            out = NULL;
        } else {
            nyx_log(NYX_LOG_ERROR, "Failed to create workflow context");
        }
        nyx_wf_free(wf);
        nyx_output_free(out);
        nyx_cli_free_result(cli);
        return 1;
    }

    /* Record workflow config in output envelope */
    {
        nyx_json_t *config = nyx_json_object();
        nyx_json_set(config, "workflow_file", nyx_json_string(wf_path));
        if (wf->id) nyx_json_set(config, "workflow_id", nyx_json_string(wf->id));
        if (wf->name) nyx_json_set(config, "workflow_name", nyx_json_string(wf->name));
        nyx_json_set(config, "step_count", nyx_json_int((long)wf->step_count));
        nyx_output_set_config(out, config);
    }

    /* Execute */
    nyx_log(NYX_LOG_INFO, "Executing workflow...");
    rc = nyx_wf_run(ctx);

    /* Build results */
    nyx_json_t *wf_results = nyx_json_object();

    /* Add workflow metadata */
    if (wf->id) nyx_json_set(wf_results, "id", nyx_json_string(wf->id));
    if (wf->name) nyx_json_set(wf_results, "name", nyx_json_string(wf->name));

    /* Add per-step results */
    nyx_json_t *steps_out = nyx_wf_get_results(ctx);
    nyx_json_set(wf_results, "steps", steps_out);

    nyx_output_set_results(out, wf_results);
    if (rc == NYX_WF_SUCCESS)
        nyx_output_set_status(out, "success");
    else
        nyx_output_set_error_msg(out, "Workflow completed with step errors.");
    nyx_output_finish(out);

    /* Human-readable summary */
    if (!nyx_output_is_json_mode(out)) {
        printf("\n");
        if (rc == NYX_WF_SUCCESS)
            nyx_log(NYX_LOG_SUCCESS, "Workflow completed successfully.");
        else
            nyx_log(NYX_LOG_ERROR, "Workflow completed with errors.");

        printf("\n  Step Summary:\n");
        for (size_t i = 0; i < wf->step_count; i++) {
            const char *status_icon;
            switch (ctx->status[i]) {
                case NYX_WF_STATUS_DONE:    status_icon = COLOR_GREEN "✓" COLOR_RESET; break;
                case NYX_WF_STATUS_SKIPPED: status_icon = COLOR_YELLOW "○" COLOR_RESET; break;
                case NYX_WF_STATUS_ERROR:   status_icon = COLOR_RED "✗" COLOR_RESET; break;
                default:                    status_icon = "?"; break;
            }
            printf("  %s %s (%s)\n", status_icon, wf->steps[i].id, wf->steps[i].tool);
        }
        printf("\n");
    }

    nyx_wf_ctx_free(ctx);
    nyx_wf_free(wf);
    nyx_output_free(out);
    nyx_cli_free_result(cli);

    return (rc == NYX_WF_SUCCESS) ? 0 : 1;
}
