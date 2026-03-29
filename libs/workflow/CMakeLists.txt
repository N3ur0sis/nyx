add_library(nyx_workflow STATIC
    src/nyx_wf_expr.c
    src/nyx_wf_parser.c
    src/nyx_wf_graph.c
    src/nyx_wf_engine.c
    src/nyx_workflow.c
)

target_include_directories(nyx_workflow
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/workflow"
)

target_link_libraries(nyx_workflow PUBLIC nyx_core nyx_output nyx_shell)
