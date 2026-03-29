/**
 * @file nyx_priv.c
 * @brief Privilege checking and auto-escalation implementation
 * @author Neur0sis (2025)
 */

#include "nyx_priv.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Linux capability bits we care about (from <linux/capability.h>).
 * Using constants directly avoids needing the kernel header.
 */
#define CAP_NET_RAW_BIT   13
#define CAP_NET_ADMIN_BIT 12

/**
 * Parse the CapEff hex string from /proc/self/status and check
 * whether the given capability bit is set.
 */
static int has_capability(int cap_bit)
{
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp)
        return 0;

    char line[256];
    unsigned long long cap_eff = 0;
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "CapEff:", 7) == 0) {
            char *hex = line + 7;
            while (*hex == ' ' || *hex == '\t')
                hex++;
            cap_eff = strtoull(hex, NULL, 16);
            found = 1;
            break;
        }
    }
    fclose(fp);

    if (!found)
        return 0;
    return (cap_eff & (1ULL << cap_bit)) != 0;
}

int nyx_priv_check(nyx_priv_t priv)
{
    if (priv == NYX_PRIV_NONE)
        return 1;

    if (geteuid() == 0)
        return 1;

    int ok = 1;
    if ((priv & NYX_PRIV_NET_RAW) && !has_capability(CAP_NET_RAW_BIT))
        ok = 0;
    if ((priv & NYX_PRIV_NET_ADMIN) && !has_capability(CAP_NET_ADMIN_BIT))
        ok = 0;

    return ok;
}

int nyx_priv_escalate(int argc, char **argv)
{
    if (!isatty(STDIN_FILENO)) {
        errno = ENOTTY;
        return -1;
    }

    /* Build new argv: { "sudo", argv[0], argv[1], ..., NULL } */
    size_t new_argc = (size_t)argc + 2;
    char **new_argv = calloc(new_argc, sizeof(char *));
    if (!new_argv)
        return -1;

    new_argv[0] = "sudo";
    for (int i = 0; i < argc; i++)
        new_argv[i + 1] = argv[i];
    new_argv[argc + 1] = NULL;

    fprintf(stderr, "\n  [*] Privilege escalation required. Executing: sudo %s\n\n", argv[0]);

    execvp("sudo", new_argv);

    /* execvp only returns on failure */
    free(new_argv);
    return -1;
}

int nyx_priv_ensure(nyx_priv_t priv, int argc, char **argv)
{
    if (nyx_priv_check(priv))
        return 0;

    return nyx_priv_escalate(argc, argv);
}

const char *nyx_priv_label(nyx_priv_t priv)
{
    if (priv == NYX_PRIV_NONE)
        return "none";

    unsigned p = (unsigned)priv;
    if ((p & (unsigned)NYX_PRIV_NET_RAW) && (p & (unsigned)NYX_PRIV_NET_ADMIN))
        return "CAP_NET_RAW + CAP_NET_ADMIN (root)";
    if (p & (unsigned)NYX_PRIV_NET_RAW)
        return "CAP_NET_RAW (root)";
    if (p & (unsigned)NYX_PRIV_NET_ADMIN)
        return "CAP_NET_ADMIN (root)";
    return "elevated";
}
