add_library(nyx_output STATIC
    src/nyx_json.c
    src/nyx_output.c
)

target_include_directories(nyx_output
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/output"
)

target_link_libraries(nyx_output PUBLIC nyx_core)
