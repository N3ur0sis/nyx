/**
 * @file ph_macspoof_cmd.c
 * @brief Command layer for macspoof
 */

#include "ph_macspoof_cmd.h"
#include "ph_macspoof_api.h"
#include "nyx_tool_registry.h"
#include "nyx_error.h"
#include "nyx_logger.h"
#include "nyx_iface.h"
#include "nyx_priv.h"
#include "nyx_version.h"

#include <string.h>

static const char *json_str(const nyx_json_t *obj, const char *key)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    return (v && nyx_json_type(v) == NYX_JSON_STRING)
               ? nyx_json_get_string(v) : NULL;
}

int ph_macspoof_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out)
{
    const char *op = json_str(params, "operation");
    if (!op) op = "show";

    const char *iface = json_str(params, "i");
    if (!iface) iface = json_str(params, "interface");

    nyx_json_t *jcfg = nyx_json_object();
    nyx_json_set(jcfg, "operation", nyx_json_string(op));
    if (iface) nyx_json_set(jcfg, "interface", nyx_json_string(iface));
    nyx_output_set_config(out, jcfg);

    if (strcmp(op, "list") == 0) {
        nyx_json_t *results = nyx_json_object();
        nyx_json_t *ifaces_arr = nyx_json_array();

        char names[64][16];
        size_t count = 0;
        int ok = (nyx_iface_list(names, 64, &count) == NYX_IFACE_SUCCESS);
        if (ok) {
            for (size_t i = 0; i < count; i++) {
                nyx_json_t *e = nyx_json_object();
                char mac[18] = {0}, ipv4[16] = {0}, netmask[16] = {0};
                nyx_iface_get_mac(names[i], mac, sizeof(mac));
                nyx_iface_get_ipv4(names[i], ipv4, sizeof(ipv4));
                nyx_iface_get_netmask(names[i], netmask, sizeof(netmask));
                nyx_json_set(e, "name", nyx_json_string(names[i]));
                nyx_json_set(e, "mac", nyx_json_string(mac));
                nyx_json_set(e, "ipv4", nyx_json_string(ipv4));
                nyx_json_set(e, "netmask", nyx_json_string(netmask));
                nyx_json_set(e, "up", nyx_json_bool(nyx_iface_is_up(names[i])));
                nyx_json_append(ifaces_arr, e);
            }
        }
        nyx_json_set(results, "interfaces", ifaces_arr);
        nyx_output_set_results(out, results);
        nyx_output_set_status(out, ok ? "success" : "error");
        if (!ok) nyx_output_set_error_from_ctx(out);
        return ok ? 0 : -1;
    }

    if (!iface || !iface[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_NO_IFACE,
                         NYX_ERROR_SEV_ERROR, "No interface specified",
                         "Provide 'i' or 'interface' parameter");
        nyx_output_set_error_from_ctx(out);
        return PH_ERR_NO_IFACE;
    }

    int ret = PH_SUCCESS;
    nyx_json_t *results = nyx_json_object();
    nyx_json_set(results, "interface", nyx_json_string(iface));

    if (strcmp(op, "show") == 0) {
        char mac[PH_MAX_MAC_LEN];
        ret = ph_macspoof_get_current_mac(iface, mac, sizeof(mac));
        if (ret == PH_SUCCESS) {
            nyx_json_set(results, "mac", nyx_json_string(mac));
            nyx_json_set(results, "up", nyx_json_bool(nyx_iface_is_up(iface)));
        }
    } else if (strcmp(op, "random") == 0) {
        char old_mac[PH_MAX_MAC_LEN] = {0};
        ph_macspoof_get_current_mac(iface, old_mac, sizeof(old_mac));
        ret = ph_macspoof_random_mac(iface);
        if (ret == PH_SUCCESS) {
            char new_mac[PH_MAX_MAC_LEN] = {0};
            ph_macspoof_get_current_mac(iface, new_mac, sizeof(new_mac));
            nyx_json_set(results, "old_mac", nyx_json_string(old_mac));
            nyx_json_set(results, "new_mac", nyx_json_string(new_mac));
            nyx_json_set(results, "verified", nyx_json_bool(1));
        }
    } else if (strcmp(op, "custom") == 0) {
        const char *mac = json_str(params, "m");
        if (!mac) mac = json_str(params, "mac");
        if (!mac || !mac[0]) {
            NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC,
                             NYX_ERROR_SEV_ERROR, "No MAC address provided",
                             "Provide 'm' or 'mac' parameter");
            nyx_json_free(results);
            nyx_output_set_error_from_ctx(out);
            return PH_ERR_INVALID_MAC;
        }
        char old_mac[PH_MAX_MAC_LEN] = {0};
        ph_macspoof_get_current_mac(iface, old_mac, sizeof(old_mac));
        ret = ph_macspoof_custom_mac(iface, mac);
        if (ret == PH_SUCCESS) {
            nyx_json_set(results, "old_mac", nyx_json_string(old_mac));
            nyx_json_set(results, "new_mac", nyx_json_string(mac));
            nyx_json_set(results, "verified", nyx_json_bool(1));
        }
    } else if (strcmp(op, "restore") == 0) {
        ret = ph_macspoof_restore_mac(iface);
        if (ret == PH_SUCCESS) {
            char mac[PH_MAX_MAC_LEN] = {0};
            ph_macspoof_get_current_mac(iface, mac, sizeof(mac));
            nyx_json_set(results, "restored_mac", nyx_json_string(mac));
            nyx_json_set(results, "verified", nyx_json_bool(1));
        }
    } else {
        NYX_ERROR_SET_EX(NYX_DOMAIN_MACSPOOF, PH_ERR_INVALID_MAC,
                         NYX_ERROR_SEV_ERROR, "Unknown operation",
                         "Use: show, random, custom, restore, or list");
        nyx_json_free(results);
        nyx_output_set_error_from_ctx(out);
        return -1;
    }

    if (ret != PH_SUCCESS) {
        nyx_json_free(results);
        nyx_output_set_error_from_ctx(out);
    } else {
        nyx_output_set_results(out, results);
        nyx_output_set_status(out, "success");
    }
    return ret;
}

/* ---- Interactive REPL command handlers ---- */

#define TOOL    "macspoof"
#define MODULE  "phobos"

static int invoke_op(const char *operation, int argc, char **argv)
{
    nyx_json_t *params = nyx_json_object();
    nyx_json_set(params, "operation", nyx_json_string(operation));

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0)
            && i + 1 < argc)
            nyx_json_set(params, "interface", nyx_json_string(argv[++i]));
        else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mac") == 0)
                 && i + 1 < argc)
            nyx_json_set(params, "mac", nyx_json_string(argv[++i]));
    }

    nyx_output_ctx_t *out = nyx_output_init(TOOL, MODULE, NYX_VERSION);
    int rc = ph_macspoof_cmd_invoke(params, out);
    if (rc != 0)
        nyx_error_log(NYX_LOG_ERROR, 0);
    nyx_output_free(out);
    nyx_json_free(params);
    return rc;
}

static int repl_show(int argc, char **argv, void *data)
{
    (void)data;
    const char *iface = NULL;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interface") == 0)
            && i + 1 < argc)
            iface = argv[++i];
    }
    if (!iface || !iface[0]) {
        nyx_log(NYX_LOG_ERROR, "No interface specified. Usage: show -i <iface>");
        return -1;
    }
    return ph_macspoof_show_mac(iface);
}

static int repl_random(int argc, char **argv, void *data)
{
    (void)data;
    return invoke_op("random", argc, argv);
}

static int repl_set(int argc, char **argv, void *data)
{
    (void)data;
    return invoke_op("custom", argc, argv);
}

static int repl_restore(int argc, char **argv, void *data)
{
    (void)data;
    return invoke_op("restore", argc, argv);
}

static int repl_list(int argc, char **argv, void *data)
{
    (void)argc; (void)argv; (void)data;
    return ph_macspoof_list_interfaces_stdout();
}

static const nyx_repl_flag_t iface_flags[] = {
    { "-i",          NYX_COMPL_IFACE },
    { "--interface",  NYX_COMPL_IFACE },
};

static const nyx_repl_flag_t set_flags[] = {
    { "-i",          NYX_COMPL_IFACE },
    { "--interface",  NYX_COMPL_IFACE },
    { "-m",          NYX_COMPL_NONE  },
    { "--mac",       NYX_COMPL_NONE  },
};

const nyx_repl_cmd_t ph_macspoof_repl_cmds[] = {
    {
        .name = "show",
        .usage = "show -i <iface>",
        .description = "Show current MAC address",
        .help =
            "  Options:\n"
            "    -i, --interface <name>  Network interface [required]\n"
            "\n"
            "  Example:\n"
            "    show -i eth0\n",
        .handler = repl_show,
        .flags = iface_flags,
        .flag_count = sizeof(iface_flags) / sizeof(iface_flags[0])
    },
    {
        .name = "random",
        .usage = "random -i <iface>",
        .description = "Set a random MAC address",
        .help =
            "  Generates a random locally-administered unicast MAC and applies it.\n"
            "  The original MAC is backed up for later restore.\n"
            "\n"
            "  Options:\n"
            "    -i, --interface <name>  Network interface [required]\n"
            "\n"
            "  Example:\n"
            "    random -i wlan0\n"
            "\n"
            "  Note: Requires root privileges.\n",
        .handler = repl_random,
        .flags = iface_flags,
        .flag_count = sizeof(iface_flags) / sizeof(iface_flags[0])
    },
    {
        .name = "set",
        .usage = "set -i <iface> -m <mac>",
        .description = "Set a custom MAC address",
        .help =
            "  Options:\n"
            "    -i, --interface <name>  Network interface [required]\n"
            "    -m, --mac <addr>        MAC address in XX:XX:XX:XX:XX:XX format [required]\n"
            "\n"
            "  Example:\n"
            "    set -i eth0 -m 00:11:22:33:44:55\n"
            "\n"
            "  Note: Requires root privileges.\n",
        .handler = repl_set,
        .flags = set_flags,
        .flag_count = sizeof(set_flags) / sizeof(set_flags[0])
    },
    {
        .name = "restore",
        .usage = "restore -i <iface>",
        .description = "Restore original MAC address",
        .help =
            "  Restores the original (permanent) MAC from backup.\n"
            "\n"
            "  Options:\n"
            "    -i, --interface <name>  Network interface [required]\n"
            "\n"
            "  Example:\n"
            "    restore -i wlan0\n"
            "\n"
            "  Note: Requires root privileges.\n",
        .handler = repl_restore,
        .flags = iface_flags,
        .flag_count = sizeof(iface_flags) / sizeof(iface_flags[0])
    },
    {
        .name = "list",
        .usage = "list",
        .description = "List network interfaces",
        .help =
            "  Shows all network interfaces with their MAC, IP, netmask and state.\n"
            "  Use this to find the right interface name for other commands.\n",
        .handler = repl_list
    },
};

const size_t ph_macspoof_repl_cmd_count =
    sizeof(ph_macspoof_repl_cmds) / sizeof(ph_macspoof_repl_cmds[0]);

void ph_macspoof_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name          = "macspoof",
        .module        = "phobos",
        .version       = NYX_VERSION,
        .description   = "MAC address spoofing (change / randomize / restore)",
        .invoke        = ph_macspoof_cmd_invoke,
        .cmds          = ph_macspoof_repl_cmds,
        .cmd_count     = sizeof(ph_macspoof_repl_cmds) / sizeof(ph_macspoof_repl_cmds[0]),
        .required_priv = NYX_PRIV_NET_ADMIN
    });
}
