cmake_minimum_required(VERSION 3.14)
project(IPCTests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Google Test setup
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(../backend/src)
include_directories(../backend/src/common)
include_directories(../backend/src/ipc)
include_directories(../backend/src/server)

# Unit tests
add_executable(
  unit_tests
  unit/test_shmem.cpp
  unit/test_coordinator.cpp  
  unit/test_http_server.cpp
  ../backend/src/common/logger.cpp
  ../backend/src/ipc/shmem_manager.cpp
  ../backend/src/ipc/pipe_manager.cpp
  ../backend/src/ipc/socket_manager.cpp
  ../backend/src/ipc/ipc_coordinator.cpp
  ../backend/src/server/http_server.cpp
)

target_link_libraries(
  unit_tests
  gtest_main
  pthread
)

# Integration tests
add_executable(
  integration_tests
  integration/test_full_flow.cpp
  ../backend/src/common/logger.cpp
  ../backend/src/ipc/shmem_manager.cpp
  ../backend/src/ipc/pipe_manager.cpp
  ../backend/src/ipc/socket_manager.cpp
  ../backend/src/ipc/ipc_coordinator.cpp
  ../backend/src/server/http_server.cpp
)

target_link_libraries(
  integration_tests
  gtest_main
  pthread
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
gtest_discover_tests(integration_tests)