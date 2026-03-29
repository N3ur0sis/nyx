/**
 * @file ph_arpspoof_cmd.h
 * @brief Command layer for arpspoof -- bridges JSON params to tool impl
 */

#ifndef PH_ARPSPOOF_CMD_H
#define PH_ARPSPOOF_CMD_H

#include "nyx_json.h"
#include "nyx_output.h"
#include "nyx_repl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run arpspoof from JSON params and populate the output context.
 * Expected params: { "i": "iface", "t": "target_ip", "s": "spoof_ip",
 *                    "b": true, "n": interval }
 */
int ph_arpspoof_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out);

/** Interactive REPL commands for arpspoof. */
extern const nyx_repl_cmd_t ph_arpspoof_repl_cmds[];
extern const size_t ph_arpspoof_repl_cmd_count;

/** Register arpspoof in the global tool registry. */
void ph_arpspoof_register(void);

#ifdef __cplusplus
}
#endif

#endif /* PH_ARPSPOOF_CMD_H */
