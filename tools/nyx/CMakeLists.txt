add_executable(nyx
    src/nyx_main.c
    src/nyx_cmd_session.c
    src/nyx_tools_builtin.c
)

target_include_directories(nyx PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(nyx PRIVATE
    nyx_shell
    nyx_workflow
    nyx_pingsweep
    nyx_portscan
    nyx_macspoof
    nyx_arpspoof
)

set_target_properties(nyx PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${NYX_BIN_DIR}"
)

file(GLOB MAN_PAGES "${CMAKE_CURRENT_SOURCE_DIR}/man/*.1")
if(MAN_PAGES)
    install(FILES ${MAN_PAGES} DESTINATION share/man/man1)
endif()

install(TARGETS nyx RUNTIME DESTINATION bin)
