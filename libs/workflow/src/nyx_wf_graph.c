/**
 * @file nyx_wf_graph.c
 * @brief DAG operations for the NYX workflow engine
 * @author Neur0sis (2025)
 *
 * Implements Kahn's algorithm for topological sort, cycle detection,
 * and workflow validation.
 */

#include <stdlib.h>
#include <string.h>

#include "nyx_wf_types.h"
#include "nyx_logger.h"

/* ====================================================================
 *  Topological sort using Kahn's algorithm -- O(V+E)
 * ==================================================================== */

int nyx_wf_topo_sort(const nyx_workflow_t *wf, size_t **order, size_t *order_len)
{
    if (!wf || !order || !order_len)
        return NYX_WF_ERR_PARAM;

    size_t n = wf->step_count;
    if (n == 0) {
        *order = NULL;
        *order_len = 0;
        return NYX_WF_SUCCESS;
    }

    /* Compute in-degree for each node */
    size_t *in_deg = calloc(n, sizeof(size_t));
    if (!in_deg)
        return NYX_WF_ERR_MEMORY;

    for (size_t i = 0; i < n; i++) {
        in_deg[i] = wf->steps[i].dep_count;
    }

    /* Build reverse adjacency: for each step, which downstream steps depend on it */
    size_t *downstream_count = calloc(n, sizeof(size_t));
    size_t **downstream = calloc(n, sizeof(size_t *));
    if (!downstream_count || !downstream) {
        free(in_deg);
        free(downstream_count);
        free(downstream);
        return NYX_WF_ERR_MEMORY;
    }

    for (size_t i = 0; i < n; i++) {
        for (size_t d = 0; d < wf->steps[i].dep_count; d++) {
            size_t dep = wf->steps[i].deps[d];
            downstream_count[dep]++;
        }
    }
    for (size_t i = 0; i < n; i++) {
        if (downstream_count[i] > 0) {
            downstream[i] = calloc(downstream_count[i], sizeof(size_t));
            downstream_count[i] = 0;
        }
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t d = 0; d < wf->steps[i].dep_count; d++) {
            size_t dep = wf->steps[i].deps[d];
            downstream[dep][downstream_count[dep]++] = i;
        }
    }

    /* BFS queue */
    size_t *queue = malloc(n * sizeof(size_t));
    size_t *result = malloc(n * sizeof(size_t));
    if (!queue || !result) {
        free(in_deg);
        free(queue);
        free(result);
        for (size_t i = 0; i < n; i++)
            free(downstream[i]);
        free(downstream);
        free(downstream_count);
        return NYX_WF_ERR_MEMORY;
    }

    size_t q_head = 0, q_tail = 0, r_len = 0;

    /* Enqueue all nodes with in-degree 0 */
    for (size_t i = 0; i < n; i++) {
        if (in_deg[i] == 0)
            queue[q_tail++] = i;
    }

    while (q_head < q_tail) {
        size_t u = queue[q_head++];
        result[r_len++] = u;

        for (size_t d = 0; d < downstream_count[u]; d++) {
            size_t v = downstream[u][d];
            in_deg[v]--;
            if (in_deg[v] == 0)
                queue[q_tail++] = v;
        }
    }

    /* Cleanup temporaries */
    free(in_deg);
    free(queue);
    for (size_t i = 0; i < n; i++)
        free(downstream[i]);
    free(downstream);
    free(downstream_count);

    /* Cycle detection */
    if (r_len != n) {
        nyx_log(NYX_LOG_ERROR, "Workflow contains a cycle (sorted %zu of %zu steps)", r_len, n);
        free(result);
        return NYX_WF_ERR_CYCLE;
    }

    *order = result;
    *order_len = r_len;
    return NYX_WF_SUCCESS;
}

/* ====================================================================
 *  Validation
 * ==================================================================== */

int nyx_wf_validate(const nyx_workflow_t *wf)
{
    if (!wf)
        return NYX_WF_ERR_PARAM;

    /* Check all step IDs are unique */
    for (size_t i = 0; i < wf->step_count; i++) {
        if (!wf->steps[i].id || !wf->steps[i].id[0]) {
            nyx_log(NYX_LOG_ERROR, "Step %zu has no ID", i);
            return NYX_WF_ERR_VALIDATE;
        }
        if (!wf->steps[i].tool || !wf->steps[i].tool[0]) {
            nyx_log(NYX_LOG_ERROR, "Step '%s' has no tool", wf->steps[i].id);
            return NYX_WF_ERR_VALIDATE;
        }
        for (size_t j = i + 1; j < wf->step_count; j++) {
            if (wf->steps[j].id && strcmp(wf->steps[i].id, wf->steps[j].id) == 0) {
                nyx_log(NYX_LOG_ERROR, "Duplicate step ID: '%s'", wf->steps[i].id);
                return NYX_WF_ERR_VALIDATE;
            }
        }
    }

    /* Check all dependency references point to existing steps */
    for (size_t i = 0; i < wf->step_count; i++) {
        for (size_t d = 0; d < wf->steps[i].dep_count; d++) {
            if (wf->steps[i].deps[d] >= wf->step_count) {
                nyx_log(NYX_LOG_ERROR, "Step '%s' has invalid dependency index %zu",
                        wf->steps[i].id, wf->steps[i].deps[d]);
                return NYX_WF_ERR_MISSING_REF;
            }
        }

        /* Self-dependency check */
        for (size_t d = 0; d < wf->steps[i].dep_count; d++) {
            if (wf->steps[i].deps[d] == i) {
                nyx_log(NYX_LOG_ERROR, "Step '%s' depends on itself", wf->steps[i].id);
                return NYX_WF_ERR_CYCLE;
            }
        }
    }

    /* Cycle detection via topological sort */
    size_t *order = NULL;
    size_t order_len = 0;
    int ret = nyx_wf_topo_sort(wf, &order, &order_len);
    free(order);

    return ret;
}
