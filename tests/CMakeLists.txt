# =============================================================================
# CMakeLists.txt para TESTES do Sistema IPC
# =============================================================================
# Este arquivo configura os testes unitários e de integração para o projeto.
# Usa Google Test (GTest) para framework de testes automáticos.
# 
# TIPOS DE TESTE:
# - Unit Tests: Testam classes individuais isoladamente
# - Integration Tests: Testam fluxo completo end-to-end
#
# DEPENDÊNCIAS:
# - Google Test (GTest) instalado no sistema
# - Todas as classes do backend (linkadas estaticamente)
# - pthread para suporte a multi-threading
# =============================================================================

cmake_minimum_required(VERSION 3.14)  # Versão mínima do CMake
project(IPCTests)  # Nome do projeto de testes

# ========== Configuração da Linguagem ==========
set(CMAKE_CXX_STANDARD 23)        # Usa C++23 (mesmo do projeto principal)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Torna C++23 obrigatório

# ========== Setup do Framework de Testes ==========
find_package(GTest REQUIRED)  # Procura Google Test no sistema
include(GoogleTest)           # Inclui funções úteis do GTest

# ========== Diretórios de Include ==========
# Permite que os testes incluam headers do backend facilmente
include_directories(../backend/src)         # Diretório raiz do código
include_directories(../backend/src/common)  # Logger e utilitários
include_directories(../backend/src/ipc)     # Managers IPC
include_directories(../backend/src/server)  # Servidor HTTP

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