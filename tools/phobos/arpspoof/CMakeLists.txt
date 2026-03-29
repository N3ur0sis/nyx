nyx_add_tool(arpspoof
    LIB_SOURCES
        src/ph_arpspoof_impl.c
        src/ph_arpspoof_cmd.c
    MAIN_SOURCE
        src/ph_arpspoof_cli.c
    EXTRA_LIBS ""
)
