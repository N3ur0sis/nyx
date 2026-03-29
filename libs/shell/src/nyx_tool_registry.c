/**
 * @file nyx_tool_registry.c
 * @brief Global tool registry for in-process tool invocation
 * @author Neur0sis (2025)
 */

#include "nyx_tool_registry.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 16

static nyx_tool_entry_t *g_entries;
static size_t g_count;
static size_t g_cap;

static char *safe_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d)
        memcpy(d, s, len + 1);
    return d;
}

int nyx_tool_registry_add(const nyx_tool_entry_t *entry)
{
    if (!entry || !entry->name || !entry->invoke)
        return -1;

    if (nyx_tool_registry_find(entry->name))
        return -1;

    if (g_count == g_cap) {
        size_t nc = g_cap ? g_cap * 2 : INITIAL_CAP;
        nyx_tool_entry_t *tmp = realloc(g_entries, nc * sizeof(*tmp));
        if (!tmp)
            return -1;
        g_entries = tmp;
        g_cap = nc;
    }

    nyx_tool_entry_t *dst = &g_entries[g_count++];
    dst->name = safe_strdup(entry->name);
    dst->module = safe_strdup(entry->module);
    dst->version = safe_strdup(entry->version);
    dst->description = safe_strdup(entry->description);
    dst->invoke = entry->invoke;
    dst->cmds = entry->cmds;
    dst->cmd_count = entry->cmd_count;
    dst->required_priv = entry->required_priv;
    return 0;
}

const nyx_tool_entry_t *nyx_tool_registry_find(const char *name)
{
    if (!name)
        return NULL;
    for (size_t i = 0; i < g_count; i++) {
        if (strcmp(g_entries[i].name, name) == 0)
            return &g_entries[i];
    }
    return NULL;
}

size_t nyx_tool_registry_count(void)
{
    return g_count;
}

const nyx_tool_entry_t *nyx_tool_registry_at(size_t index)
{
    return (index < g_count) ? &g_entries[index] : NULL;
}

void nyx_tool_registry_cleanup(void)
{
    for (size_t i = 0; i < g_count; i++) {
        free((char *)g_entries[i].name);
        free((char *)g_entries[i].module);
        free((char *)g_entries[i].version);
        free((char *)g_entries[i].description);
    }
    free(g_entries);
    g_entries = NULL;
    g_count = g_cap = 0;
}
