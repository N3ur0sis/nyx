/**
 * @file nyx_tools_builtin.c
 * @brief Registers all built-in tools with the global registry
 *
 * This file is compiled into binaries that need in-process tool access
 * (nyx, nyx-run).  Each tool's _register() function is declared as an
 * extern and called explicitly -- no magic, easy to extend.
 */

#include "nyx_tool_registry.h"

extern void ph_pingsweep_register(void);
extern void ph_portscan_register(void);
extern void ph_macspoof_register(void);
extern void ph_arpspoof_register(void);

void nyx_tools_register_all(void)
{
    ph_pingsweep_register();
    ph_portscan_register();
    ph_macspoof_register();
    ph_arpspoof_register();
}
