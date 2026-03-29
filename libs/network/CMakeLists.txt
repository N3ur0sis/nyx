add_library(nyx_network STATIC
    src/nyx_iface.c
    src/nyx_netaddr.c
    src/nyx_packet.c
    src/nyx_socket.c
)

target_include_directories(nyx_network
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include/network"
)

target_link_libraries(nyx_network
    PUBLIC nyx_core
)
