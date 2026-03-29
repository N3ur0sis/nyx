#
# NyxHelpers.cmake -- convenience functions for the NYX build system
#

#
# nyx_add_tool(<name>
#   [LIB_SOURCES src1.c src2.c ...]
#   [MAIN_SOURCE main.c]
#   [EXTRA_LIBS lib1 lib2 ...]
# )
#
# Creates two targets:
#   nyx_<name>   -- STATIC library from LIB_SOURCES (impl + cmd layer)
#   nyx-<name>   -- executable from MAIN_SOURCE, linked to the library
#
# The library is linked against nyx_core, nyx_network, nyx_output by
# default, plus any EXTRA_LIBS.  The executable additionally links
# nyx_shell for interactive REPL support.
#
# If LIB_SOURCES / MAIN_SOURCE are not given, falls back to the legacy
# "glob all src/*.c" behavior for backwards compatibility.
#
function(nyx_add_tool TOOL_NAME)
    cmake_parse_arguments(ARG "" "MAIN_SOURCE" "LIB_SOURCES;EXTRA_LIBS" ${ARGN})

    set(LIB_TARGET  "nyx_${TOOL_NAME}")
    set(EXE_TARGET  "nyx-${TOOL_NAME}")

    # -- New split-target mode --
    if(ARG_LIB_SOURCES)
        # Tool implementation library
        add_library(${LIB_TARGET} STATIC ${ARG_LIB_SOURCES})
        target_include_directories(${LIB_TARGET} PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(${LIB_TARGET} PUBLIC
            nyx_core
            nyx_network
            nyx_output
            nyx_shell
            ${ARG_EXTRA_LIBS}
        )

        if(ARG_MAIN_SOURCE)
            add_executable(${EXE_TARGET} ${ARG_MAIN_SOURCE})
            target_include_directories(${EXE_TARGET} PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src"
            )
            target_link_libraries(${EXE_TARGET} PRIVATE
                ${LIB_TARGET}
                nyx_shell
            )
            set_target_properties(${EXE_TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${NYX_BIN_DIR}"
            )
            install(TARGETS ${EXE_TARGET} RUNTIME DESTINATION bin)
        endif()

    else()
        # Legacy mode: glob all .c, single executable
        file(GLOB TOOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
        if(NOT TOOL_SOURCES)
            message(WARNING "nyx_add_tool(${TOOL_NAME}): no .c files found")
            return()
        endif()

        add_executable(${EXE_TARGET} ${TOOL_SOURCES})
        target_include_directories(${EXE_TARGET} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
        target_link_libraries(${EXE_TARGET} PRIVATE
            nyx_core
            nyx_network
            nyx_output
            ${ARG_EXTRA_LIBS}
        )
        set_target_properties(${EXE_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${NYX_BIN_DIR}"
        )
        install(TARGETS ${EXE_TARGET} RUNTIME DESTINATION bin)
    endif()

    # Man pages
    file(GLOB MAN_PAGES "${CMAKE_CURRENT_SOURCE_DIR}/man/*.8")
    if(MAN_PAGES)
        install(FILES ${MAN_PAGES} DESTINATION share/man/man8)
    endif()
endfunction()
