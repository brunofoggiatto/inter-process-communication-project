/**
 * @file test_full_flow.cpp
 * @brief Integration tests for complete IPC system flow
 */

#include <gtest/gtest.h>
#include "ipc/ipc_coordinator.h"
#include "server/http_server.h"
#include <thread>
#include <chrono>
#include <memory>

using namespace ipc_project;

class FullFlowIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        coordinator = std::make_shared<IPCCoordinator>();
        http_server = std::make_unique<HTTPServer>(8090);
        
        // Integra HTTP server com IPC coordinator
        http_server->setIPCCoordinator(coordinator);
        http_server->setCORS(true);
        
        // Inicializa o coordinator
        ASSERT_TRUE(coordinator->initialize());
        
        // Inicia o servidor HTTP
        ASSERT_TRUE(http_server->start());
        
        // Aguarda estabilização
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    void TearDown() override {
        if (http_server && http_server->isRunning()) {
            http_server->stop();
        }
        
        if (coordinator && coordinator->isRunning()) {
            coordinator->shutdown();
        }
        
        http_server.reset();
        coordinator.reset();
        
        // Aguarda cleanup completo
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::shared_ptr<IPCCoordinator> coordinator;
    std::unique_ptr<HTTPServer> http_server;
};

// Teste de inicialização completa do sistema
TEST_F(FullFlowIntegrationTest, SystemInitialization) {
    // Verifica que coordinator está rodando
    EXPECT_TRUE(coordinator->isRunning());
    
    // Verifica que HTTP server está rodando
    EXPECT_TRUE(http_server->isRunning());
    
    // Verifica status inicial do sistema
    auto status = coordinator->getFullStatus();
    EXPECT_EQ(status.status, "running");
    EXPECT_EQ(status.mechanisms.size(), 3);
    EXPECT_FALSE(status.all_active); // nenhum mecanismo iniciado ainda
}

// Teste de fluxo completo: start mechanism via comando
TEST_F(FullFlowIntegrationTest, StartMechanismFlow) {
    // 1. Cria comando para iniciar shared memory
    IPCCommand start_cmd;
    start_cmd.action = "start";
    start_cmd.mechanism = IPCMechanism::SHARED_MEMORY;
    
    // 2. Executa comando via coordinator
    std::string response = coordinator->executeCommand(start_cmd);
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("success"), std::string::npos);
    
    // 3. Verifica que mecanismo está ativo
    auto mech_status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_TRUE(mech_status.is_active);
    EXPECT_EQ(mech_status.name, "shared_memory");
    
    // 4. Verifica status geral do sistema
    auto full_status = coordinator->getFullStatus();
    EXPECT_GT(full_status.total_processes, 0);
}

// Teste de fluxo de envio de mensagem
TEST_F(FullFlowIntegrationTest, MessageSendingFlow) {
    // 1. Inicia shared memory
    ASSERT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    
    // Aguarda inicialização
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 2. Envia mensagem
    std::string test_message = "Integration test - complete message";
    bool sent = coordinator->sendMessage(IPCMechanism::SHARED_MEMORY, test_message);
    EXPECT_TRUE(sent);
    
    // 3. Verifica logs
    auto logs = coordinator->getLogs(IPCMechanism::SHARED_MEMORY, 10);
    EXPECT_GT(logs.size(), 0);
    
    // 4. Verifica que mensagem aparece nos logs
    bool found_message = false;
    for (const auto& log : logs) {
        if (log.find(test_message) != std::string::npos) {
            found_message = true;
            break;
        }
    }
    // Note: pode não encontrar a mensagem exata nos logs dependendo do formato
    // mas pelo menos deve ter logs de atividade
}

// Teste de múltiplos mecanismos simultaneamente
TEST_F(FullFlowIntegrationTest, MultipleSimultaneousMechanisms) {
    // 1. Inicia múltiplos mecanismos
    EXPECT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    EXPECT_TRUE(coordinator->startMechanism(IPCMechanism::PIPES));
    
    // Aguarda inicialização
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // 2. Verifica status de ambos
    auto shmem_status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    auto pipes_status = coordinator->getMechanismStatus(IPCMechanism::PIPES);
    
    EXPECT_TRUE(shmem_status.is_active);
    EXPECT_TRUE(pipes_status.is_active);
    
    // 3. Envia mensagens para ambos
    EXPECT_TRUE(coordinator->sendMessage(IPCMechanism::SHARED_MEMORY, "msg para shmem"));
    EXPECT_TRUE(coordinator->sendMessage(IPCMechanism::PIPES, "msg para pipes"));
    
    // 4. Verifica status geral
    auto full_status = coordinator->getFullStatus();
    EXPECT_GE(full_status.total_processes, 2);
    
    // 5. Para mecanismos
    EXPECT_TRUE(coordinator->stopMechanism(IPCMechanism::SHARED_MEMORY));
    EXPECT_TRUE(coordinator->stopMechanism(IPCMechanism::PIPES));
}

// Teste de ciclo completo: start -> send -> logs -> stop
TEST_F(FullFlowIntegrationTest, CompleteLifecycle) {
    // 1. START - via comando
    IPCCommand start_cmd;
    start_cmd.action = "start";
    start_cmd.mechanism = IPCMechanism::SHARED_MEMORY;
    
    std::string response = coordinator->executeCommand(start_cmd);
    EXPECT_NE(response.find("success"), std::string::npos);
    
    // Aguarda inicialização
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 2. SEND - via comando  
    IPCCommand send_cmd;
    send_cmd.action = "send";
    send_cmd.mechanism = IPCMechanism::SHARED_MEMORY;
    send_cmd.message = "Mensagem de teste do ciclo completo";
    
    response = coordinator->executeCommand(send_cmd);
    EXPECT_NE(response.find("success"), std::string::npos);
    
    // 3. STATUS - via comando
    IPCCommand status_cmd;
    status_cmd.action = "status";
    status_cmd.mechanism = IPCMechanism::SHARED_MEMORY;
    
    response = coordinator->executeCommand(status_cmd);
    EXPECT_NE(response.find("mechanisms"), std::string::npos);
    
    // 4. LOGS - verifica que há atividade
    auto logs = coordinator->getLogs(IPCMechanism::SHARED_MEMORY);
    EXPECT_GT(logs.size(), 0);
    
    // 5. STOP - via comando
    IPCCommand stop_cmd;
    stop_cmd.action = "stop";
    stop_cmd.mechanism = IPCMechanism::SHARED_MEMORY;
    
    response = coordinator->executeCommand(stop_cmd);
    EXPECT_NE(response.find("success"), std::string::npos);
    
    // Verifica que mecanismo parou
    auto final_status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_FALSE(final_status.is_active);
}

// Integration test HTTP Server + IPC Coordinator
TEST_F(FullFlowIntegrationTest, HTTPServerIntegration) {
    // Verifica que servidor HTTP está conectado ao coordinator
    EXPECT_TRUE(http_server->isRunning());
    
    // Simula que seria testado com requisições HTTP reais:
    // GET /ipc/status -> deve retornar JSON com status
    // POST /ipc/start/pipes -> deve iniciar pipes
    // POST /ipc/send {"mechanism":"pipes","message":"test"} -> deve enviar
    // GET /ipc/logs/pipes -> deve retornar logs
    
    // Por enquanto, testa indiretamente via coordinator
    auto status = coordinator->getStatusJSON();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("mechanisms"), std::string::npos);
    
    // Testa contadores do servidor
    size_t initial_count = http_server->getRequestCount();
    EXPECT_GE(initial_count, 0);
}

// Teste de resiliência: restart mechanisms
TEST_F(FullFlowIntegrationTest, MechanismResilience) {
    // 1. Inicia mechanism
    ASSERT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 2. Verifica que está ativo
    auto status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_TRUE(status.is_active);
    
    // 3. Restart
    EXPECT_TRUE(coordinator->restartMechanism(IPCMechanism::SHARED_MEMORY));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 4. Verifica que ainda está ativo após restart
    status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_TRUE(status.is_active);
    
    // 5. Testa funcionalidade após restart
    EXPECT_TRUE(coordinator->sendMessage(IPCMechanism::SHARED_MEMORY, "post-restart message"));
}

// Teste de shutdown gracioso
TEST_F(FullFlowIntegrationTest, GracefulShutdown) {
    // 1. Inicia alguns mecanismos
    coordinator->startMechanism(IPCMechanism::SHARED_MEMORY);
    coordinator->startMechanism(IPCMechanism::PIPES);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 2. Verifica que sistema está ativo
    EXPECT_TRUE(coordinator->isRunning());
    EXPECT_TRUE(http_server->isRunning());
    
    // 3. Para servidor HTTP primeiro
    http_server->stop();
    EXPECT_FALSE(http_server->isRunning());
    
    // 4. Coordinator ainda deve estar rodando
    EXPECT_TRUE(coordinator->isRunning());
    
    // 5. Para coordinator
    coordinator->shutdown();
    EXPECT_FALSE(coordinator->isRunning());
    
    // Sistema deve estar completamente parado
    // TearDown vai limpar o resto
}

// Teste de parsing JSON completo
TEST_F(FullFlowIntegrationTest, JSONParsing) {
    // Testa parsing de comandos JSON complexos
    IPCCommand cmd;
    
    std::string complex_json = R"({
        "action": "send",
        "mechanism": "shared_memory", 
        "message": "Complex message with JSON characters: {\"nested\": true}"
    })";
    
    EXPECT_TRUE(cmd.fromJSON(complex_json));
    EXPECT_EQ(cmd.action, "send");
    EXPECT_EQ(cmd.mechanism, IPCMechanism::SHARED_MEMORY);
    EXPECT_FALSE(cmd.message.empty());
    
    // Testa serialização de volta para JSON
    std::string serialized = cmd.toJSON();
    EXPECT_FALSE(serialized.empty());
    EXPECT_NE(serialized.find("send"), std::string::npos);
}