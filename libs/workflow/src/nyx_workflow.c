/**
 * @file nyx_workflow.c
 * @brief Public API helpers for the NYX workflow engine
 * @author Neur0sis (2025)
 *
 * Provides convenience functions that compose the parser, graph,
 * expression, and engine modules. Most heavy lifting is delegated
 * to the respective compilation units.
 */

#include <stdlib.h>
#include <string.h>

#include "nyx_workflow.h"
#include "nyx_json.h"

void nyx_wf_set_var(nyx_workflow_t *wf, const char *key, const char *value)
{
    if (!wf || !key || !value)
        return;

    if (!wf->vars)
        wf->vars = nyx_json_object();

    /* Try to parse value as JSON (number, bool, null) */
    if (strcmp(value, "true") == 0) {
        nyx_json_set(wf->vars, key, nyx_json_bool(1));
    } else if (strcmp(value, "false") == 0) {
        nyx_json_set(wf->vars, key, nyx_json_bool(0));
    } else if (strcmp(value, "null") == 0) {
        nyx_json_set(wf->vars, key, nyx_json_null());
    } else {
        /* Try numeric */
        char *end;
        long lv = strtol(value, &end, 10);
        if (*end == '\0' && end != value) {
            nyx_json_set(wf->vars, key, nyx_json_int(lv));
            return;
        }
        double dv = strtod(value, &end);
        if (*end == '\0' && end != value) {
            nyx_json_set(wf->vars, key, nyx_json_real(dv));
            return;
        }
        nyx_json_set(wf->vars, key, nyx_json_string(value));
    }
}
