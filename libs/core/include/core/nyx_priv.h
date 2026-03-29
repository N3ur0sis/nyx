/**
 * @file nyx_priv.h
 * @brief Privilege checking and auto-escalation for NYX tools
 * @author Neur0sis (2025)
 *
 * NYX tools that need raw sockets or interface configuration require
 * elevated privileges.  This module provides a portable way to check
 * the current privilege level and, when running interactively,
 * transparently re-execute the binary via sudo.
 */

#ifndef NYX_PRIV_H
#define NYX_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Privilege requirements.
 *
 * NYX_PRIV_NONE: tool works without any special privilege.
 * NYX_PRIV_NET_RAW: raw/packet sockets (pingsweep, portscan SYN, arpspoof).
 * NYX_PRIV_NET_ADMIN: interface configuration (macspoof).
 */
typedef enum {
    NYX_PRIV_NONE = 0,
    NYX_PRIV_NET_RAW = (1 << 0),
    NYX_PRIV_NET_ADMIN = (1 << 1),
} nyx_priv_t;

/**
 * Check whether the current process holds the requested privilege(s).
 *
 * First checks euid == 0 (root), then falls back to reading the
 * effective capability set from /proc/self/status (no libcap dep).
 *
 * @param priv  Bitmask of required privileges
 * @return 1 if all requested privileges are available, 0 otherwise
 */
int nyx_priv_check(nyx_priv_t priv);

/**
 * Re-execute the current binary with sudo prepended.
 *
 * Only works when stdin is a TTY (interactive mode).  In non-TTY
 * contexts (workflows, scripts) this returns -1 immediately.
 *
 * On success this function does NOT return (the process is replaced).
 * On failure it returns -1 with errno set.
 *
 * @param argc  Original argc from main()
 * @param argv  Original argv from main()
 * @return -1 on failure (never returns on success)
 */
int nyx_priv_escalate(int argc, char **argv);

/**
 * Check whether the requested privilege is satisfied; if not and stdin
 * is a TTY, attempt to re-exec with sudo.
 *
 * Typical usage at the top of a tool's main():
 *
 *   if (nyx_priv_ensure(NYX_PRIV_NET_RAW, argc, argv) != 0) {
 *       fprintf(stderr, "This tool requires root privileges.\n");
 *       return 1;
 *   }
 *
 * @param priv  Required privilege bitmask
 * @param argc  main() argc
 * @param argv  main() argv
 * @return 0 if privilege is satisfied, -1 if escalation failed/impossible
 */
int nyx_priv_ensure(nyx_priv_t priv, int argc, char **argv);

/**
 * Return a human-readable label for the given privilege requirement.
 * Useful for log/error messages.
 */
const char *nyx_priv_label(nyx_priv_t priv);

#ifdef __cplusplus
}
#endif

#endif /* NYX_PRIV_H */
