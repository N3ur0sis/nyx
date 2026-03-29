add_library(nyx_shell STATIC
    src/nyx_repl.c
    src/nyx_tool_registry.c
    vendor/linenoise/linenoise.c
)

set_source_files_properties(vendor/linenoise/linenoise.c PROPERTIES
    COMPILE_FLAGS "-Wno-conversion -Wno-sign-conversion -Wno-shadow -Wno-old-style-definition"
)

target_include_directories(nyx_shell PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/shell"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/linenoise"
)

target_link_libraries(nyx_shell PUBLIC
    nyx_core
    nyx_output
)
