/**
 * @file ph_arpspoof_cmd.c
 * @brief Command layer for arpspoof
 */

#include "ph_arpspoof_cmd.h"
#include "ph_arpspoof_api.h"
#include "nyx_tool_registry.h"
#include "nyx_error.h"
#include "nyx_logger.h"
#include "nyx_priv.h"
#include "nyx_version.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
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

static int json_bool(const nyx_json_t *obj, const char *key, int def)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    if (v && nyx_json_type(v) == NYX_JSON_BOOL)
        return nyx_json_get_bool(v);
    return def;
}

static ph_arpspoof_session_t *g_arp_session;

static void arp_signal_handler(int sig)
{
    (void)sig;
    if (g_arp_session)
        ph_arpspoof_stop(g_arp_session);
}

int ph_arpspoof_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out)
{
    const char *iface = json_str(params, "i");
    if (!iface)
        iface = json_str(params, "interface");
    const char *target = json_str(params, "t");
    if (!target)
        target = json_str(params, "target");
    const char *spoof = json_str(params, "s");
    if (!spoof)
        spoof = json_str(params, "spoof");

    if (!iface || !iface[0] || !target || !target[0] || !spoof || !spoof[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_INVALID_PARAM, NYX_ERROR_SEV_ERROR,
                         "Missing required parameters (interface, target, spoof)",
                         "Provide 'i', 't', and 's' parameters");
        nyx_output_set_error_from_ctx(out);
        return PH_ARPSPOOF_ERR_INVALID_PARAM;
    }

    if (geteuid() != 0) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_ARPSPOOF, PH_ARPSPOOF_ERR_PERMISSION, NYX_ERROR_SEV_ERROR,
                         "ARP spoofing requires root privileges", "Run with sudo");
        nyx_output_set_error_from_ctx(out);
        return PH_ARPSPOOF_ERR_PERMISSION;
    }

    ph_arpspoof_config_t cfg = {0};
    strncpy(cfg.iface, iface, sizeof(cfg.iface) - 1);
    strncpy(cfg.target_ip, target, sizeof(cfg.target_ip) - 1);
    strncpy(cfg.spoof_ip, spoof, sizeof(cfg.spoof_ip) - 1);
    cfg.bidirectional = json_bool(params, "b", json_bool(params, "bidirectional", 0));
    cfg.interval =
        (int)json_int(params, "n", json_int(params, "interval", PH_ARPSPOOF_DEFAULT_INTERVAL));

    nyx_json_t *jcfg = nyx_json_object();
    nyx_json_set(jcfg, "interface", nyx_json_string(cfg.iface));
    nyx_json_set(jcfg, "target_ip", nyx_json_string(cfg.target_ip));
    nyx_json_set(jcfg, "spoof_ip", nyx_json_string(cfg.spoof_ip));
    nyx_json_set(jcfg, "bidirectional", nyx_json_bool(cfg.bidirectional));
    nyx_json_set(jcfg, "interval", nyx_json_int(cfg.interval));
    nyx_output_set_config(out, jcfg);

    ph_arpspoof_session_t *session = NULL;
    int ret = ph_arpspoof_init(&cfg, &session);
    if (ret != PH_ARPSPOOF_SUCCESS) {
        nyx_output_set_error_from_ctx(out);
        return ret;
    }

    g_arp_session = session;
    struct sigaction sa = {.sa_handler = arp_signal_handler};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct timeval t0;
    gettimeofday(&t0, NULL);

    ret = ph_arpspoof_start(session);

    struct timeval t1;
    gettimeofday(&t1, NULL);
    double dur = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_usec - t0.tv_usec) / 1e6;

    int restore_ok = (ph_arpspoof_restore(session) == PH_ARPSPOOF_SUCCESS);

    nyx_json_t *results = nyx_json_object();
    nyx_json_set(results, "duration_s", nyx_json_real(dur));
    nyx_json_set(results, "restored", nyx_json_bool(restore_ok));
    nyx_output_set_results(out, results);

    if (ret != PH_ARPSPOOF_SUCCESS)
        nyx_output_set_error_from_ctx(out);
    else
        nyx_output_set_status(out, "success");

    ph_arpspoof_cleanup(session);
    g_arp_session = NULL;

    struct sigaction def = {.sa_handler = SIG_DFL};
    sigaction(SIGINT, &def, NULL);
    sigaction(SIGTERM, &def, NULL);

    return ret;
}

/* ---- Interactive REPL command handlers ---- */

#define TOOL   "arpspoof"
#define MODULE "phobos"

static int repl_start(int argc, char **argv, void *data)
{
    (void)data;
    nyx_json_t *params = nyx_json_object();
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0) && i + 1 < argc)
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

    nyx_output_ctx_t *out = nyx_output_init(TOOL, MODULE, NYX_VERSION);
    int rc = ph_arpspoof_cmd_invoke(params, out);
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
    return ph_arpspoof_list_interfaces_stdout();
}

static const nyx_repl_flag_t start_flags[] = {
    {"-i", NYX_COMPL_IFACE}, {"--interface", NYX_COMPL_IFACE},
    {"-t", NYX_COMPL_NONE},  {"--target", NYX_COMPL_NONE},
    {"-s", NYX_COMPL_NONE},  {"--spoof", NYX_COMPL_NONE},
    {"-b", NYX_COMPL_NONE},  {"--bidirectional", NYX_COMPL_NONE},
    {"-n", NYX_COMPL_NONE},  {"--interval", NYX_COMPL_NONE},
};

const nyx_repl_cmd_t ph_arpspoof_repl_cmds[] = {
    {.name = "start",
     .usage = "start -i <iface> -t <target> -s <spoof> [-b] [-n sec]",
     .description = "Start ARP poisoning",
     .help = "  Sends crafted ARP replies to poison the target's ARP cache,\n"
             "  making it associate the spoof IP with your MAC address.\n"
             "  Press Ctrl-C to stop; ARP tables are restored on exit.\n"
             "\n"
             "  Options:\n"
             "    -i, --interface <name>  Network interface [required]\n"
             "    -t, --target <ip>       Victim IP address [required]\n"
             "    -s, --spoof <ip>        IP address to impersonate [required]\n"
             "                            Usually the gateway (e.g. 192.168.1.1)\n"
             "    -b, --bidirectional     Poison both target and spoof host\n"
             "    -n, --interval <sec>    Seconds between ARP packets (default: 1)\n"
             "\n"
             "  Examples:\n"
             "    start -i eth0 -t 192.168.1.50 -s 192.168.1.1\n"
             "    start -i wlan0 -t 10.0.0.5 -s 10.0.0.1 -b -n 2\n"
             "\n"
             "  Note: Requires root privileges.\n",
     .handler = repl_start,
     .flags = start_flags,
     .flag_count = sizeof(start_flags) / sizeof(start_flags[0])},
    {.name = "list",
     .usage = "list",
     .description = "List available network interfaces",
     .help = "  Shows all network interfaces with their IP and MAC addresses.\n"
             "  Use this to find the right interface name for the -i option.\n",
     .handler = repl_list},
};

const size_t ph_arpspoof_repl_cmd_count =
    sizeof(ph_arpspoof_repl_cmds) / sizeof(ph_arpspoof_repl_cmds[0]);

void ph_arpspoof_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name = "arpspoof",
        .module = "phobos",
        .version = NYX_VERSION,
        .description = "ARP cache poisoning for traffic interception",
        .invoke = ph_arpspoof_cmd_invoke,
        .cmds = ph_arpspoof_repl_cmds,
        .cmd_count = sizeof(ph_arpspoof_repl_cmds) / sizeof(ph_arpspoof_repl_cmds[0]),
        .required_priv = NYX_PRIV_NET_RAW});
}
