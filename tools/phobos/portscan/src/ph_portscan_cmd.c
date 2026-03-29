/**
 * @file ph_portscan_cmd.c
 * @brief Command layer for portscan
 */

#include "ph_portscan_cmd.h"
#include "ph_portscan_api.h"
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
    return (v && nyx_json_type(v) == NYX_JSON_STRING)
               ? nyx_json_get_string(v) : NULL;
}

static long json_int(const nyx_json_t *obj, const char *key, long def)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    if (v && nyx_json_type(v) == NYX_JSON_INT) return nyx_json_get_int(v);
    if (v && nyx_json_type(v) == NYX_JSON_STRING) {
        char *end;
        long val = strtol(nyx_json_get_string(v), &end, 10);
        if (*end == '\0') return val;
    }
    return def;
}

static int json_bool(const nyx_json_t *obj, const char *key, int def)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    if (v && nyx_json_type(v) == NYX_JSON_BOOL) return nyx_json_get_bool(v);
    return def;
}

static int parse_port_range(const char *str, uint16_t *start, uint16_t *end)
{
    if (!str || !start || !end) return -1;
    char *dash = strchr(str, '-');
    if (!dash) {
        char *ep;
        long val = strtol(str, &ep, 10);
        if (*ep != '\0' || val < 1 || val > 65535) return -1;
        *start = (uint16_t)val;
        *end   = (uint16_t)val;
        return 0;
    }
    char buf[32];
    size_t plen = (size_t)(dash - str);
    if (plen == 0 || plen >= sizeof(buf)) return -1;
    memcpy(buf, str, plen);
    buf[plen] = '\0';
    char *ep;
    long s = strtol(buf, &ep, 10);
    if (*ep || s < 1 || s > 65535) return -1;
    long e = strtol(dash + 1, &ep, 10);
    if (*ep || e < 1 || e > 65535) return -1;
    *start = (uint16_t)s;
    *end   = (uint16_t)e;
    return 0;
}

int ph_portscan_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out)
{
    const char *target = json_str(params, "t");
    if (!target) target = json_str(params, "target");

    if (!target || !target[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_INVALID_TARGET,
                         NYX_ERROR_SEV_ERROR, "No target IP specified",
                         "Provide 't' or 'target' parameter");
        nyx_output_set_error_from_ctx(out);
        return PH_PORTSCAN_ERR_INVALID_TARGET;
    }

    ph_portscan_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.target, sizeof(cfg.target), "%s", target);

    const char *port_str = json_str(params, "p");
    if (!port_str) port_str = json_str(params, "ports");

    if (port_str) {
        if (parse_port_range(port_str, &cfg.port_start, &cfg.port_end) != 0) {
            NYX_ERROR_SET_EX(NYX_DOMAIN_PORTSCAN, PH_PORTSCAN_ERR_INVALID_PARAM,
                             NYX_ERROR_SEV_ERROR, "Invalid port range",
                             "Use format: start-end (e.g. 1-1024)");
            nyx_output_set_error_from_ctx(out);
            return PH_PORTSCAN_ERR_INVALID_PARAM;
        }
    } else {
        cfg.top_ports = (int)json_int(params, "P",
                             json_int(params, "top-ports",
                                      PH_PORTSCAN_DEFAULT_TOP_PORTS));
    }

    const char *mode_str = json_str(params, "m");
    if (!mode_str) mode_str = json_str(params, "mode");
    if (mode_str && (strcmp(mode_str, "syn") == 0 || strcmp(mode_str, "SYN") == 0))
        cfg.mode = PH_PORTSCAN_TCP_SYN;
    else if (mode_str)
        cfg.mode = PH_PORTSCAN_TCP_CONNECT;
    else
        cfg.mode = (geteuid() == 0) ? PH_PORTSCAN_TCP_SYN
                                    : PH_PORTSCAN_TCP_CONNECT;

    cfg.threads    = (int)json_int(params, "T",
                          json_int(params, "threads",
                                   PH_PORTSCAN_DEFAULT_THREADS));
    cfg.timeout_ms = (int)json_int(params, "w",
                          json_int(params, "timeout",
                                   PH_PORTSCAN_DEFAULT_TIMEOUT));

    int open_only = json_bool(params, "o",
                       json_bool(params, "open-only", 0));

    nyx_json_t *jcfg = nyx_json_object();
    nyx_json_set(jcfg, "target", nyx_json_string(cfg.target));
    if (cfg.top_ports > 0)
        nyx_json_set(jcfg, "top_ports", nyx_json_int(cfg.top_ports));
    else {
        nyx_json_set(jcfg, "port_start", nyx_json_int(cfg.port_start));
        nyx_json_set(jcfg, "port_end", nyx_json_int(cfg.port_end));
    }
    nyx_json_set(jcfg, "mode", nyx_json_string(
        cfg.mode == PH_PORTSCAN_TCP_SYN ? "syn" : "connect"));
    nyx_json_set(jcfg, "timeout_ms", nyx_json_int(cfg.timeout_ms));
    nyx_json_set(jcfg, "threads", nyx_json_int(cfg.threads));
    nyx_output_set_config(out, jcfg);

    ph_portscan_result_t *result = NULL;
    int rc = ph_portscan_scan(&cfg, &result);

    if (rc != PH_PORTSCAN_SUCCESS) {
        nyx_output_set_error_from_ctx(out);
        return rc;
    }

    nyx_json_t *results = nyx_json_object();
    nyx_json_set(results, "target", nyx_json_string(result->target));

    nyx_json_t *ports = nyx_json_array();
    for (size_t i = 0; i < result->scanned_count; i++) {
        if (open_only && result->ports[i].state != PH_PORT_OPEN)
            continue;
        const char *state;
        switch (result->ports[i].state) {
            case PH_PORT_OPEN:     state = "open";     break;
            case PH_PORT_CLOSED:   state = "closed";   break;
            case PH_PORT_FILTERED: state = "filtered"; break;
            default:               state = "unknown";  break;
        }
        nyx_json_t *p = nyx_json_object();
        nyx_json_set(p, "port", nyx_json_int(result->ports[i].port));
        nyx_json_set(p, "state", nyx_json_string(state));
        nyx_json_set(p, "protocol", nyx_json_string("tcp"));
        nyx_json_append(ports, p);
    }
    nyx_json_set(results, "ports", ports);
    nyx_json_set(results, "open_count", nyx_json_int((long)result->open_count));
    nyx_json_set(results, "scanned_count", nyx_json_int((long)result->scanned_count));
    nyx_json_set(results, "elapsed_ms", nyx_json_real(result->elapsed_ms));
    nyx_json_set(results, "scan_mode", nyx_json_string(
        result->actual_mode == PH_PORTSCAN_TCP_SYN ? "syn" : "connect"));

    nyx_output_set_results(out, results);
    nyx_output_set_status(out, "success");

    ph_portscan_free_result(result);
    return 0;
}

/* ---- Interactive REPL command handlers ---- */

#define TOOL    "portscan"
#define MODULE  "phobos"

static int repl_scan(int argc, char **argv, void *data)
{
    (void)data;
    nyx_json_t *params = nyx_json_object();
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0)
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

    nyx_output_ctx_t *out = nyx_output_init(TOOL, MODULE, NYX_VERSION);
    int rc = ph_portscan_cmd_invoke(params, out);
    if (rc != 0)
        nyx_error_log(NYX_LOG_ERROR, 0);
    nyx_output_free(out);
    nyx_json_free(params);
    return rc;
}

static const nyx_repl_flag_t scan_flags[] = {
    { "-t",          NYX_COMPL_NONE },
    { "--target",    NYX_COMPL_NONE },
    { "-p",          NYX_COMPL_NONE },
    { "--ports",     NYX_COMPL_NONE },
    { "-P",          NYX_COMPL_NONE },
    { "--top-ports", NYX_COMPL_NONE },
    { "-m",          NYX_COMPL_NONE },
    { "--mode",      NYX_COMPL_NONE },
    { "-T",          NYX_COMPL_NONE },
    { "--threads",   NYX_COMPL_NONE },
    { "-w",          NYX_COMPL_NONE },
    { "--timeout",   NYX_COMPL_NONE },
    { "-o",          NYX_COMPL_NONE },
    { "--open-only", NYX_COMPL_NONE },
};

const nyx_repl_cmd_t ph_portscan_repl_cmds[] = {
    {
        .name = "scan",
        .usage = "scan -t <ip> [options]",
        .description = "Run TCP port scan on a target host",
        .help =
            "  Options:\n"
            "    -t, --target <ip>       Target IP address [required]\n"
            "    -p, --ports <range>     Port range to scan (e.g. 1-1024, 80)\n"
            "    -P, --top-ports <N>     Scan top N common ports (default: 100)\n"
            "                            Ignored when -p is specified\n"
            "    -m, --mode <mode>       Scan mode: 'connect' or 'syn' (default: auto)\n"
            "                            SYN mode requires root, connect works as user\n"
            "    -T, --threads <N>       Number of concurrent threads (default: 16)\n"
            "    -w, --timeout <ms>      Timeout per port in milliseconds (default: 2000)\n"
            "    -o, --open-only         Only show open ports in output\n"
            "\n"
            "  Examples:\n"
            "    scan -t 192.168.1.1\n"
            "    scan -t 10.0.0.5 -p 1-65535 -m syn -T 64\n"
            "    scan -t 10.0.0.5 -P 20 -o\n"
            "\n"
            "  Note: SYN scan requires root privileges.\n",
        .handler = repl_scan,
        .flags = scan_flags,
        .flag_count = sizeof(scan_flags) / sizeof(scan_flags[0])
    },
};

const size_t ph_portscan_repl_cmd_count =
    sizeof(ph_portscan_repl_cmds) / sizeof(ph_portscan_repl_cmds[0]);

void ph_portscan_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name          = "portscan",
        .module        = "phobos",
        .version       = NYX_VERSION,
        .description   = "TCP port scanner (Connect / SYN half-open)",
        .invoke        = ph_portscan_cmd_invoke,
        .cmds          = ph_portscan_repl_cmds,
        .cmd_count     = sizeof(ph_portscan_repl_cmds) / sizeof(ph_portscan_repl_cmds[0]),
        .required_priv = NYX_PRIV_NONE
    });
}
