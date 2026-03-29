/**
 * @file ph_macspoof_cmd.h
 * @brief Command layer for macspoof -- bridges JSON params to tool impl
 */

#ifndef PH_MACSPOOF_CMD_H
#define PH_MACSPOOF_CMD_H

#include "nyx_json.h"
#include "nyx_output.h"
#include "nyx_repl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run macspoof from JSON params and populate the output context.
 * Expected params: { "i": "iface", "operation": "show|random|custom|restore|list",
 *                    "m": "XX:XX:XX:XX:XX:XX" }
 */
int ph_macspoof_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out);

/** Interactive REPL commands for macspoof. */
extern const nyx_repl_cmd_t ph_macspoof_repl_cmds[];
extern const size_t         ph_macspoof_repl_cmd_count;

/** Register macspoof in the global tool registry. */
void ph_macspoof_register(void);

#ifdef __cplusplus
}
#endif

#endif /* PH_MACSPOOF_CMD_H */
