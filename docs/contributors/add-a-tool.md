# Adding a New Tool

This guide walks through creating a new NYX tool from scratch. The example creates a fictional `dnslookup` tool in the `phobos` module.

## 1. Create the Directory Structure

```
tools/phobos/dnslookup/
├── CMakeLists.txt
├── src/
│   ├── ph_dnslookup_api.h
│   ├── ph_dnslookup_impl.c
│   ├── ph_dnslookup_cmd.h
│   ├── ph_dnslookup_cmd.c
│   └── ph_dnslookup_cli.c
└── man/
    └── nyx-dnslookup.8
```

## 2. Define the API (`ph_dnslookup_api.h`)

The API header defines config and result structs used by the implementation. Keep it free of framework dependencies.

```c
#ifndef PH_DNSLOOKUP_API_H
#define PH_DNSLOOKUP_API_H

#define PH_DNSLOOKUP_SUCCESS           0
#define PH_DNSLOOKUP_ERR_PARAM        -1
#define PH_DNSLOOKUP_ERR_RESOLVE      -2

typedef struct {
    char domain[256];
    int timeout_ms;
} ph_dnslookup_config_t;

typedef struct {
    char domain[256];
    char ipv4[16];
    double elapsed_ms;
} ph_dnslookup_result_t;

int ph_dnslookup_resolve(const ph_dnslookup_config_t *cfg,
                          ph_dnslookup_result_t *result);

#endif
```

## 3. Write the Implementation (`ph_dnslookup_impl.c`)

Pure tool logic. Uses shared libraries (`nyx_error`, `nyx_netaddr`, etc.) for common operations but has no JSON or CLI dependencies.

```c
#include "ph_dnslookup_api.h"
#include "nyx_error.h"
#include <string.h>
// ... implementation ...

int ph_dnslookup_resolve(const ph_dnslookup_config_t *cfg,
                          ph_dnslookup_result_t *result)
{
    if (!cfg->domain[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_CORE, PH_DNSLOOKUP_ERR_PARAM,
                         NYX_ERROR_SEV_ERROR, "No domain specified",
                         "Provide a domain name");
        return PH_DNSLOOKUP_ERR_PARAM;
    }
    // ... resolve the domain ...
    return PH_DNSLOOKUP_SUCCESS;
}
```

## 4. Write the Command Layer (`ph_dnslookup_cmd.c`)

This is the tool's contract with the framework. It:

- Parses JSON parameters
- Calls the implementation
- Builds the output envelope
- Defines REPL commands
- Registers the tool

```c
#include "ph_dnslookup_cmd.h"
#include "ph_dnslookup_api.h"
#include "nyx_tool_registry.h"
#include "nyx_error.h"
#include "nyx_logger.h"
#include "nyx_priv.h"
#include <string.h>

static const char *json_str(const nyx_json_t *obj, const char *key)
{
    const nyx_json_t *v = nyx_json_get(obj, key);
    return (v && nyx_json_type(v) == NYX_JSON_STRING)
               ? nyx_json_get_string(v) : NULL;
}

int ph_dnslookup_cmd_invoke(const nyx_json_t *params, nyx_output_ctx_t *out)
{
    const char *domain = json_str(params, "d");
    if (!domain) domain = json_str(params, "domain");

    if (!domain || !domain[0]) {
        NYX_ERROR_SET_EX(NYX_DOMAIN_CORE, PH_DNSLOOKUP_ERR_PARAM,
                         NYX_ERROR_SEV_ERROR, "No domain specified",
                         "Provide 'd' or 'domain' parameter");
        nyx_output_set_error_from_ctx(out);
        return PH_DNSLOOKUP_ERR_PARAM;
    }

    ph_dnslookup_config_t cfg = {0};
    strncpy(cfg.domain, domain, sizeof(cfg.domain) - 1);

    /* Record config in the envelope */
    nyx_json_t *jcfg = nyx_json_object();
    nyx_json_set(jcfg, "domain", nyx_json_string(cfg.domain));
    nyx_output_set_config(out, jcfg);

    /* Run the implementation */
    ph_dnslookup_result_t result = {0};
    int rc = ph_dnslookup_resolve(&cfg, &result);
    if (rc != PH_DNSLOOKUP_SUCCESS) {
        nyx_output_set_error_from_ctx(out);
        return rc;
    }

    /* Build results */
    nyx_json_t *results = nyx_json_object();
    nyx_json_set(results, "domain", nyx_json_string(result.domain));
    nyx_json_set(results, "ipv4", nyx_json_string(result.ipv4));
    nyx_json_set(results, "elapsed_ms", nyx_json_real(result.elapsed_ms));
    nyx_output_set_results(out, results);
    nyx_output_set_status(out, "success");

    return 0;
}

/* ---- REPL ---- */

static int repl_resolve(int argc, char **argv, void *data)
{
    (void)data;
    nyx_json_t *params = nyx_json_object();
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--domain") == 0)
            && i + 1 < argc)
            nyx_json_set(params, "domain", nyx_json_string(argv[++i]));
    }
    nyx_output_ctx_t *out = nyx_output_init("dnslookup", "phobos", "1.0");
    int rc = ph_dnslookup_cmd_invoke(params, out);
    if (rc != 0) nyx_error_log(NYX_LOG_ERROR, 0);
    nyx_output_free(out);
    nyx_json_free(params);
    return rc;
}

static const nyx_repl_flag_t resolve_flags[] = {
    { "-d",       NYX_COMPL_NONE },
    { "--domain", NYX_COMPL_NONE },
};

const nyx_repl_cmd_t ph_dnslookup_repl_cmds[] = {
    {
        .name = "resolve",
        .usage = "resolve -d <domain>",
        .description = "Resolve a domain name to an IP",
        .help =
            "  Options:\n"
            "    -d, --domain <name>  Domain to resolve [required]\n"
            "\n"
            "  Example:\n"
            "    resolve -d example.com\n",
        .handler = repl_resolve,
        .flags = resolve_flags,
        .flag_count = sizeof(resolve_flags) / sizeof(resolve_flags[0])
    },
};

const size_t ph_dnslookup_repl_cmd_count =
    sizeof(ph_dnslookup_repl_cmds) / sizeof(ph_dnslookup_repl_cmds[0]);

void ph_dnslookup_register(void)
{
    nyx_tool_registry_add(&(nyx_tool_entry_t){
        .name          = "dnslookup",
        .module        = "phobos",
        .version       = "1.0",
        .description   = "DNS domain resolution",
        .invoke        = ph_dnslookup_cmd_invoke,
        .cmds          = ph_dnslookup_repl_cmds,
        .cmd_count     = ph_dnslookup_repl_cmd_count,
        .required_priv = NYX_PRIV_NONE
    });
}
```

## 5. Write the CLI Frontend (`ph_dnslookup_cli.c`)

```c
#include "ph_dnslookup_cmd.h"
#include "nyx_repl.h"
#include "nyx_output.h"
#include "nyx_priv.h"

int main(int argc, char *argv[])
{
    if (nyx_output_argv_has_json(argc, argv))
        return run_oneshot(argc, argv);  /* handle -J mode */

    /* Uncomment if the tool needs root:
    if (nyx_priv_ensure(NYX_PRIV_NET_RAW, argc, argv) != 0) {
        fprintf(stderr, "This tool requires root privileges.\n");
        return 1;
    }
    */

    nyx_repl_t *repl = nyx_repl_create("dnslookup");
    nyx_repl_set_welcome(repl,
        "Type 'resolve -d <domain>' to look up a domain, 'help' for all commands.");
    nyx_repl_add_cmds(repl, ph_dnslookup_repl_cmds, ph_dnslookup_repl_cmd_count);
    nyx_repl_run(repl);
    nyx_repl_free(repl);
    return 0;
}
```

## 6. Write the CMakeLists.txt

```cmake
nyx_add_tool(dnslookup
    LIB_SOURCES
        src/ph_dnslookup_impl.c
        src/ph_dnslookup_cmd.c
    MAIN_SOURCE
        src/ph_dnslookup_cli.c
)
```

Then add `add_subdirectory(dnslookup)` to `tools/phobos/CMakeLists.txt`.

## 7. Register the Tool

Add the extern declaration and registration call to `tools/nyx/src/nyx_tools_builtin.c`:

```c
extern void ph_dnslookup_register(void);

void nyx_tools_register_all(void)
{
    // ... existing tools ...
    ph_dnslookup_register();
}
```

Link the tool library in `tools/nyx/CMakeLists.txt` and `tools/nyx-run/CMakeLists.txt`:

```cmake
target_link_libraries(nyx PRIVATE nyx_dnslookup ...)
target_link_libraries(nyx-run PRIVATE nyx_dnslookup ...)
```

## 8. Add a Man Page

Create `man/nyx-dnslookup.8` following the existing tool man pages as a template. CMake installs it automatically via `nyx_add_tool()`.

## 9. Add Documentation

Create `docs/tools/dnslookup.md` following the format of the existing tool pages. Document accepted parameters, result fields, workflow examples, and error conditions.

## Checklist

- [ ] `*_api.h` defines config/result structs with no framework deps
- [ ] `*_impl.c` uses `NYX_ERROR_SET_EX` for all failure paths
- [ ] `*_cmd.c` reads params with both short and long key aliases
- [ ] `*_cmd.c` populates config, results, and status in the output envelope
- [ ] `*_cmd.c` defines REPL commands with usage, description, help, and flags
- [ ] `*_cmd.c` calls `nyx_tool_registry_add()` with correct `required_priv`
- [ ] `*_cli.c` handles `-J` for JSON mode and enters REPL otherwise
- [ ] `nyx_tools_builtin.c` calls the new register function
- [ ] CMake links the tool library into `nyx` and `nyx-run`
- [ ] Man page exists in `man/`
- [ ] Docs page exists in `docs/tools/`
