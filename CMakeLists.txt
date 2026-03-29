cmake_minimum_required(VERSION 3.16)
project(nyx
    VERSION 1.0.0
    DESCRIPTION "NYX Offensive Security Framework"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# Generate the single-source version header from the project() version
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/nyx_version.h.in"
    "${CMAKE_BINARY_DIR}/nyx_version.h"
    @ONLY
)

# --- Security-hardened compilation flags ---
# Warnings
set(NYX_WARN_FLAGS "-Wall -Wextra -Wshadow -Wformat=2 -Wformat-security")
set(NYX_WARN_FLAGS "${NYX_WARN_FLAGS} -Wstrict-prototypes -Wold-style-definition")
set(NYX_WARN_FLAGS "${NYX_WARN_FLAGS} -Werror=format-security -Werror=implicit-function-declaration")
set(NYX_WARN_FLAGS "${NYX_WARN_FLAGS} -Wconversion -Wsign-conversion")

# Stack protection
set(NYX_HARDENING_FLAGS "-fstack-protector-strong -fstack-clash-protection")

# FORTIFY_SOURCE detects buffer overflows in string/memory functions at runtime
set(NYX_HARDENING_FLAGS "${NYX_HARDENING_FLAGS} -D_FORTIFY_SOURCE=2")

# Position-independent code (required for ASLR)
set(NYX_HARDENING_FLAGS "${NYX_HARDENING_FLAGS} -fPIE")

# Control-flow enforcement (Intel CET on supported hardware)
include(CheckCCompilerFlag)
check_c_compiler_flag("-fcf-protection" NYX_HAS_CF_PROTECTION)
if(NYX_HAS_CF_PROTECTION)
    set(NYX_HARDENING_FLAGS "${NYX_HARDENING_FLAGS} -fcf-protection")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NYX_WARN_FLAGS} ${NYX_HARDENING_FLAGS} -O2")

# Linker hardening
set(NYX_LINK_FLAGS "-Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${NYX_LINK_FLAGS}")

set(NYX_BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH
    "Output directory for tool binaries")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NYX_BIN_DIR}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(NyxHelpers)

add_subdirectory(libs/core)
add_subdirectory(libs/output)
add_subdirectory(libs/network)
add_subdirectory(libs/shell)
add_subdirectory(libs/workflow)
add_subdirectory(tools/phobos)
add_subdirectory(tools/nyx-run)
add_subdirectory(tools/nyx)
