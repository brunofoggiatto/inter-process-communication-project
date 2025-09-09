cmake_minimum_required(VERSION 3.14)
project(IPCProject VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Set build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    endif()
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/backend/src)

# Add subdirectories
add_subdirectory(backend)

# Add tests if building tests is enabled
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests)
    add_subdirectory(tests)
endif()

# Installation rules
install(TARGETS ipc_main 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Package configuration
set(CPACK_PACKAGE_NAME "IPCProject")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Inter-Process Communication System")
set(CPACK_PACKAGE_CONTACT "luiz@example.com")

include(CPack)

# Print configuration summary
message(STATUS "IPC Project Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")