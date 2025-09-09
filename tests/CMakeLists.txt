cmake_minimum_required(VERSION 3.14)
project(IPCTests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Google Test setup - use system installed version
find_package(GTest REQUIRED)
include(GoogleTest)

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
  GTest::gtest_main
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
  GTest::gtest_main
  pthread
)

gtest_discover_tests(unit_tests)
gtest_discover_tests(integration_tests)