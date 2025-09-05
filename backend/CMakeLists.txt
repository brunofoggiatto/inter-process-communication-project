cmake_minimum_required(VERSION 3.14)

# Include directories for this module
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/ipc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/server)

# Common library - shared components
add_library(ipc_common STATIC
    src/common/logger.cpp
)

target_link_libraries(ipc_common
    Threads::Threads
)

# IPC library - all IPC mechanisms
add_library(ipc_core STATIC
    src/ipc/pipe_manager.cpp
    src/ipc/socket_manager.cpp
    src/ipc/shmem_manager.cpp
    src/ipc/ipc_coordinator.cpp
)

target_link_libraries(ipc_core
    ipc_common
    Threads::Threads
)

# Server library - HTTP server
add_library(ipc_server STATIC
    src/server/http_server.cpp
)

target_link_libraries(ipc_server
    ipc_core
    ipc_common
    Threads::Threads
)

# Main executable
add_executable(ipc_main
    src/main.cpp
)

target_link_libraries(ipc_main
    ipc_server
    ipc_core
    ipc_common
    Threads::Threads
)

# Set target properties
set_target_properties(ipc_main PROPERTIES
    OUTPUT_NAME "ipc_system"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Additional libraries for IPC functionality
if(UNIX)
    target_link_libraries(ipc_core rt)  # For shared memory and semaphores
endif()

# Installation rules for libraries
install(TARGETS ipc_common ipc_core ipc_server
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# Install headers
install(DIRECTORY src/
    DESTINATION include/ipc_project
    FILES_MATCHING PATTERN "*.h"
)