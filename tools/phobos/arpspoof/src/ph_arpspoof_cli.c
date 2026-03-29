/**
 * @file ph_arpspoof_cli.c
 * @brief Interactive shell frontend for arpspoof
 * @author Neur0sis (2025)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ph_arpspoof_cmd.h"
#include "nyx_cli.h"
#include "nyx_output.h"
#include "nyx_json.h"
#include "nyx_priv.h"
#include "nyx_repl.h"
#include "nyx_version.h"

#define TOOL   "arpspoof"
#define MODULE "phobos"

static int run_oneshot(int argc, char *argv[])
{
    int json_mode = 0;
    const char *session_id = NULL;
    nyx_json_t *params = nyx_json_object();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-J") == 0 || strcmp(argv[i], "--json") == 0)
            json_mode = 1;
        else if ((strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--session") == 0) && i + 1 < argc)
            session_id = argv[++i];
        else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0) &&
                 i + 1 < argc)
            nyx_json_set(params, "interface", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0) && i + 1 < argc)
            nyx_json_set(params, "target_ip", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--spoof") == 0) && i + 1 < argc)
            nyx_json_set(params, "spoof_ip", nyx_json_string(argv[++i]));
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bidirectional") == 0)
            nyx_json_set(params, "bidirectional", nyx_json_bool(1));
        else if ((strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--interval") == 0) && i + 1 < argc)
            nyx_json_set(params, "interval", nyx_json_string(argv[++i]));
    }

    nyx_output_ctx_t *out =
        nyx_output_from_cli("nyx-" TOOL, MODULE, NYX_VERSION, json_mode, session_id);
    int rc = ph_arpspoof_cmd_invoke(params, out);
    nyx_output_finish(out);
    nyx_output_free(out);
    nyx_json_free(params);
    return rc;
}

int main(int argc, char *argv[])
{
    if (nyx_output_argv_has_json(argc, argv))
        return run_oneshot(argc, argv);

    if (nyx_priv_ensure(NYX_PRIV_NET_RAW, argc, argv) != 0) {
        fprintf(stderr,
                "arpspoof: requires root privileges (raw sockets).\n"
                "  Run with: sudo %s\n",
                argv[0]);
        return 1;
    }

    nyx_cli_banner_config_t banner = {
        .module = MODULE,
        .tool_name = TOOL,
        .version = NYX_VERSION,
        .author = "Neur0sis",
        .primary_color = NYX_CLI_COLOR_CYAN,
        .secondary_color = NYX_CLI_COLOR_YELLOW,
        .style = NYX_CLI_BANNER_ASCII_ART,
    };
    nyx_cli_print_banner(&banner);

    nyx_repl_t *repl = nyx_repl_create(TOOL);
    nyx_repl_add_cmds(repl, ph_arpspoof_repl_cmds, ph_arpspoof_repl_cmd_count);
    nyx_repl_set_welcome(repl, "Type 'start -i <iface> -t <target> -s <spoof>' to begin, "
                               "'list' for interfaces, 'help' for commands.");

    int rc = nyx_repl_run(repl);
    nyx_repl_free(repl);
    return rc;
}
