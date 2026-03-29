/**
 * @file ph_pingsweep_cmd.h
 * @brief Command layer for pingsweep -- bridges JSON params to tool impl
 */

#ifndef PH_PINGSWEEP_CMD_H
#define PH_PINGSWEEP_CMD_H

#include "nyx_json.h"
#include "nyx_output.h"
#include "nyx_repl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Run pingsweep from JSON params and populate the output context.
 * Expected params: { "c": "cidr", "i": "iface", "t": timeout, "T": threads }
 * Does NOT call nyx_output_finish().
 */
int ph_pingsweep_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out);

/** Interactive REPL commands for pingsweep. */
extern const nyx_repl_cmd_t ph_pingsweep_repl_cmds[];
extern const size_t         ph_pingsweep_repl_cmd_count;

/** Register pingsweep in the global tool registry. */
void ph_pingsweep_register(void);

#ifdef __cplusplus
}
#endif

#endif /* PH_PINGSWEEP_CMD_H */
