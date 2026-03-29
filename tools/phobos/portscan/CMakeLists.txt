nyx_add_tool(portscan
    LIB_SOURCES
        src/ph_portscan_impl.c
        src/ph_portscan_cmd.c
    MAIN_SOURCE
        src/ph_portscan_cli.c
    EXTRA_LIBS pthread
)
