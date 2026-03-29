/**
 * @file ph_portscan_cli.c
 * @brief Interactive shell frontend for portscan
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ph_portscan_cmd.h"
#include "nyx_cli.h"
#include "nyx_output.h"
#include "nyx_json.h"
#include "nyx_repl.h"
#include "nyx_version.h"

#define TOOL     "portscan"
#define MODULE   "phobos"

static int run_oneshot(int argc, char *argv[])
{
    int json_mode = 0;
    const char *session_id = NULL;
    nyx_json_t *params = nyx_json_object();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0)
            json_mode = 1;
        else if ((strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--session") == 0)
                 && i + 1 < argc)
            session_id = argv[++i];
        else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "target", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--ports") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "ports", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-P") == 0 || strcmp(argv[i], "--top-ports") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "top_ports", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "mode", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "--threads") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "threads", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--timeout") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "timeout_ms", nyx_json_string(argv[++i]));
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--open-only") == 0)
            nyx_json_set(params, "open_only", nyx_json_bool(1));
    }

    nyx_output_ctx_t *out = nyx_output_from_cli(
        "nyx-" TOOL, MODULE, NYX_VERSION, json_mode, session_id);
    int rc = ph_portscan_cmd_invoke(params, out);
    nyx_output_finish(out);
    nyx_output_free(out);
    nyx_json_free(params);
    return rc;
}

int main(int argc, char *argv[])
{
    if (nyx_output_argv_has_json(argc, argv))
        return run_oneshot(argc, argv);

    nyx_cli_banner_config_t banner = {
        .module = MODULE, .tool_name = TOOL, .version = NYX_VERSION,
        .author = "Neur0sis", .primary_color = NYX_CLI_COLOR_CYAN,
        .secondary_color = NYX_CLI_COLOR_YELLOW,
        .style = NYX_CLI_BANNER_ASCII_ART,
    };
    nyx_cli_print_banner(&banner);

    nyx_repl_t *repl = nyx_repl_create(TOOL);
    nyx_repl_add_cmds(repl, ph_portscan_repl_cmds,
                       ph_portscan_repl_cmd_count);
    nyx_repl_set_welcome(repl,
        "Type 'scan -t <ip>' to scan ports, 'help' for commands.");

    int rc = nyx_repl_run(repl);
    nyx_repl_free(repl);
    return rc;
}
