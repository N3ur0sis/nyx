add_library(nyx_core STATIC
    src/nyx_cli.c
    src/nyx_error.c
    src/nyx_logger.c
    src/nyx_priv.c
    src/nyx_term.c
)

target_include_directories(nyx_core
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include/core"
    PUBLIC  "${CMAKE_BINARY_DIR}"
)

target_link_libraries(nyx_core
    PUBLIC pthread
)
