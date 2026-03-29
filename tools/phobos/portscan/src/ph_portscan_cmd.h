/**
 * @file ph_portscan_cmd.h
 * @brief Command layer for portscan -- bridges JSON params to tool impl
 */

#ifndef PH_PORTSCAN_CMD_H
#define PH_PORTSCAN_CMD_H

#include "nyx_json.h"
#include "nyx_output.h"
#include "nyx_repl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run portscan from JSON params and populate the output context.
 * Expected params: { "t": "ip", "p": "1-1024", "P": 100, "m": "syn",
 *                    "T": 16, "w": 2000, "o": true }
 */
int ph_portscan_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out);

/** Interactive REPL commands for portscan. */
extern const nyx_repl_cmd_t ph_portscan_repl_cmds[];
extern const size_t ph_portscan_repl_cmd_count;

/** Register portscan in the global tool registry. */
void ph_portscan_register(void);

#ifdef __cplusplus
}
#endif

#endif /* PH_PORTSCAN_CMD_H */
