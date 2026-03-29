/**
 * @file ph_pingsweep_cmd.c
 * @brief Command layer for pingsweep
 */

#include "ph_pingsweep_cmd.h"
#include "ph_pingsweep_api.h"
#include "nyx_tool_registry.h"
#include "nyx_error.h"
#include "nyx_logger.h"
#include "nyx_priv.h"
#include "nyx_version.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *json_str(const nyx_json_t *obj, const char *key)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    return (v && nyx_json_type(v) == NYX_JSON_STRING) ? nyx_json_get_string(v) : NULL;
}

static long json_int(const nyx_json_t *obj, const char *key, long def)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    if (v && nyx_json_type(v) == NYX_JSON_INT)
        return nyx_json_get_int(v);
    if (v && nyx_json_type(v) == NYX_JSON_STRING) {
        char *end;
        long val = strtol(nyx_json_get_string(v), &end, 10);
        if (*end == '\0')
            return val;
    }
    return def;
}

int ph_pingsweep_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out)
{
    const char *cidr = json_str(params, "c");
    if (!cidr)
        cidr = json_str(params, "cidr");

    if (!cidr || !cidr[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_CIDR, NYX_ERROR_SEV_ERROR,
                         "No CIDR target specified", "Provide 'c' or 'cidr' parameter");
        nyx_output_set_error_from_ctx(out);
        return PH_PINGSWEEP_ERR_CIDR;
    }

    if (geteuid() != 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PINGSWEEP, PH_PINGSWEEP_ERR_PERMISSION, NYX_ERROR_SEV_ERROR,
                         "Pingsweep requires root privileges", "Run with sudo");
        nyx_output_set_error_from_ctx(out);
        return PH_PINGSWEEP_ERR_PERMISSION;
    }

    ph_pingsweep_config_t cfg = {0};
    strncpy(cfg.cidr, cidr, sizeof(cfg.cidr) - 1);

    const char *iface = json_str(params, "i");
    if (!iface)
        iface = json_str(params, "interface");
    if (iface)
        strncpy(cfg.iface, iface, sizeof(cfg.iface) - 1);

    cfg.timeout_ms =
        (int)json_int(params, "t", json_int(params, "timeout", PH_PINGSWEEP_DEFAULT_TIMEOUT));
    cfg.threads =
        (int)json_int(params, "T", json_int(params, "threads", PH_PINGSWEEP_DEFAULT_THREADS));

    nyx_json_t *jcfg = nyx_json_object();
    nyx_json_set(jcfg, "cidr", nyx_json_string(cfg.cidr));
    if (cfg.iface[0])
        nyx_json_set(jcfg, "interface", nyx_json_string(cfg.iface));
    nyx_json_set(jcfg, "timeout_ms", nyx_json_int(cfg.timeout_ms));
    nyx_json_set(jcfg, "threads", nyx_json_int(cfg.threads));
    nyx_output_set_config(out, jcfg);

    ph_pingsweep_result_t *result = NULL;
    int rc = ph_pingsweep_scan(&cfg, &result);

    if (rc != PH_PINGSWEEP_SUCCESS) {
        nyx_output_set_error_from_ctx(out);
        return rc;
    }

    nyx_json_t *results = nyx_json_object();
    nyx_json_t *hosts = nyx_json_array();
    for (size_t i = 0; i < result->total; i++) {
        nyx_json_t *h = nyx_json_object();
        nyx_json_set(h, "ip", nyx_json_string(result->hosts[i].ip));
        nyx_json_set(h, "alive", nyx_json_bool(result->hosts[i].alive));
        nyx_json_set(h, "latency_ms", nyx_json_real(result->hosts[i].latency_ms));
        nyx_json_append(hosts, h);
    }
    nyx_json_set(results, "hosts", hosts);
    nyx_json_set(results, "total", nyx_json_int((long)result->total));
    nyx_json_set(results, "alive_count", nyx_json_int((long)result->alive_count));
    nyx_json_set(results, "elapsed_ms", nyx_json_real(result->elapsed_ms));

    nyx_output_set_results(out, results);
    nyx_output_set_status(out, "success");

    ph_pingsweep_free_result(result);
    return 0;
}

/* ---- Interactive REPL command handlers ---- */

#define TOOL   "pingsweep"
#define MODULE "phobos"

static int repl_scan(int argc, char **argv, void *data)
{
    (void)data;
    nyx_json_t *params = nyx_json_object();
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cidr") == 0) && i + 1 < argc)
            nyx_json_set(params, "cidr", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0) &&
                 i + 1 < argc)
            nyx_json_set(params, "interface", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) && i + 1 < argc)
            nyx_json_set(params, "timeout_ms", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "--threads") == 0) && i + 1 < argc)
            nyx_json_set(params, "threads", nyx_json_string(argv[++i]));
    }

    nyx_output_ctx_t *out = nyx_output_init(TOOL, MODULE, NYX_VERSION);
    int rc = ph_pingsweep_cmd_invoke(params, out);
    if (rc != 0)
        nyx_error_log(NYX_LOG_ERROR, 0);
    nyx_output_free(out);
    nyx_json_free(params);
    return rc;
}

static int repl_list(int argc, char **argv, void *data)
{
    (void)argc;
    (void)argv;
    (void)data;
    return ph_pingsweep_list_interfaces_stdout();
}

static const nyx_repl_flag_t scan_flags[] = {
    {"-c", NYX_COMPL_NONE},           {"--cidr", NYX_COMPL_NONE},    {"-i", NYX_COMPL_IFACE},
    {"--interface", NYX_COMPL_IFACE}, {"-t", NYX_COMPL_NONE},        {"--timeout", NYX_COMPL_NONE},
    {"-T", NYX_COMPL_NONE},           {"--threads", NYX_COMPL_NONE},
};

const nyx_repl_cmd_t ph_pingsweep_repl_cmds[] = {
    {.name = "scan",
     .usage = "scan -c <cidr> [-i iface] [-t ms] [-T N]",
     .description = "Run ICMP ping sweep on a CIDR range",
     .help = "  Options:\n"
             "    -c, --cidr <cidr>       Target CIDR range [required]\n"
             "                            e.g. 192.168.1.0/24, 10.0.0.0/16\n"
             "    -i, --interface <name>  Network interface to use (auto-detected)\n"
             "    -t, --timeout <ms>      Timeout per host in milliseconds (default: 1000)\n"
             "    -T, --threads <N>       Number of concurrent ping threads (default: 32)\n"
             "\n"
             "  Examples:\n"
             "    scan -c 192.168.1.0/24\n"
             "    scan -c 10.0.0.0/8 -T 64 -t 500\n"
             "\n"
             "  Note: Requires root privileges (raw ICMP sockets).\n",
     .handler = repl_scan,
     .flags = scan_flags,
     .flag_count = sizeof(scan_flags) / sizeof(scan_flags[0])},
    {.name = "list",
     .usage = "list",
     .description = "List available network interfaces",
     .help = "  Shows all network interfaces with their IP and MAC addresses.\n"
             "  Use this to find the right interface name for the -i option.\n",
     .handler = repl_list},
};

const size_t ph_pingsweep_repl_cmd_count =
    sizeof(ph_pingsweep_repl_cmds) / sizeof(ph_pingsweep_repl_cmds[0]);

void ph_pingsweep_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name = "pingsweep",
        .module = "phobos",
        .version = NYX_VERSION,
        .description = "ICMP ping sweep for network host discovery",
        .invoke = ph_pingsweep_cmd_invoke,
        .cmds = ph_pingsweep_repl_cmds,
        .cmd_count = sizeof(ph_pingsweep_repl_cmds) / sizeof(ph_pingsweep_repl_cmds[0]),
        .required_priv = NYX_PRIV_NET_RAW});
}
