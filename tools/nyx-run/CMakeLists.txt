add_executable(nyx-run
    src/nyx_run_cli.c
    "${CMAKE_SOURCE_DIR}/tools/nyx/src/nyx_tools_builtin.c"
)

target_include_directories(nyx-run PRIVATE
    "${CMAKE_SOURCE_DIR}/tools/nyx/src"
)

target_link_libraries(nyx-run PRIVATE
    nyx_shell
    nyx_workflow
    nyx_pingsweep
    nyx_portscan
    nyx_macspoof
    nyx_arpspoof
)

set_target_properties(nyx-run PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${NYX_BIN_DIR}"
)

file(GLOB MAN_PAGES "${CMAKE_CURRENT_SOURCE_DIR}/man/*.8")
if(MAN_PAGES)
    install(FILES ${MAN_PAGES} DESTINATION share/man/man8)
endif()

install(TARGETS nyx-run RUNTIME DESTINATION bin)
