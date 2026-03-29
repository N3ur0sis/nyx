nyx_add_tool(pingsweep
    LIB_SOURCES
        src/ph_pingsweep_impl.c
        src/ph_pingsweep_cmd.c
    MAIN_SOURCE
        src/ph_pingsweep_cli.c
    EXTRA_LIBS pthread
)
