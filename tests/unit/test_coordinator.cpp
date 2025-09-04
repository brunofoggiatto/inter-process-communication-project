/**
 * @file test_coordinator.cpp
 * @brief Testes unitários para IPCCoordinator
 */

#include <gtest/gtest.h>
#include "ipc/ipc_coordinator.h"
#include <thread>
#include <chrono>

using namespace ipc_project;

class IPCCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        coordinator = std::make_unique<IPCCoordinator>();
    }
    
    void TearDown() override {
        if (coordinator && coordinator->isRunning()) {
            coordinator->shutdown();
        }
        coordinator.reset();
    }
    
    std::unique_ptr<IPCCoordinator> coordinator;
};

// Teste básico de inicialização
TEST_F(IPCCoordinatorTest, InitializationAndShutdown) {
    EXPECT_FALSE(coordinator->isRunning());
    
    EXPECT_TRUE(coordinator->initialize());
    EXPECT_TRUE(coordinator->isRunning());
    
    coordinator->shutdown();
    EXPECT_FALSE(coordinator->isRunning());
}

// Teste de controle de mecanismos
TEST_F(IPCCoordinatorTest, MechanismControl) {
    ASSERT_TRUE(coordinator->initialize());
    
    // Testa start dos mecanismos
    EXPECT_TRUE(coordinator->startMechanism(IPCMechanism::PIPES));
    EXPECT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    
    // Testa stop dos mecanismos
    EXPECT_TRUE(coordinator->stopMechanism(IPCMechanism::PIPES));
    EXPECT_TRUE(coordinator->stopMechanism(IPCMechanism::SHARED_MEMORY));
}

// Teste de status dos mecanismos
TEST_F(IPCCoordinatorTest, MechanismStatus) {
    ASSERT_TRUE(coordinator->initialize());
    
    // Status inicial
    auto status = coordinator->getFullStatus();
    EXPECT_EQ(status.mechanisms.size(), 3); // PIPES, SOCKETS, SHARED_MEMORY
    EXPECT_FALSE(status.all_active);
    EXPECT_EQ(status.status, "running");
    
    // Inicia um mecanismo e verifica status
    coordinator->startMechanism(IPCMechanism::SHARED_MEMORY);
    auto mech_status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_EQ(mech_status.name, "shared_memory");
    EXPECT_TRUE(mech_status.is_active);
}

// Teste de JSON status
TEST_F(IPCCoordinatorTest, JSONStatus) {
    ASSERT_TRUE(coordinator->initialize());
    
    std::string json_status = coordinator->getStatusJSON();
    EXPECT_FALSE(json_status.empty());
    
    // Verifica se contém campos esperados
    EXPECT_NE(json_status.find("mechanisms"), std::string::npos);
    EXPECT_NE(json_status.find("all_active"), std::string::npos);
    EXPECT_NE(json_status.find("status"), std::string::npos);
}

// Teste de execução de comandos
TEST_F(IPCCoordinatorTest, CommandExecution) {
    ASSERT_TRUE(coordinator->initialize());
    
    // Comando de start
    IPCCommand start_cmd;
    start_cmd.action = "start";
    start_cmd.mechanism = IPCMechanism::SHARED_MEMORY;
    
    std::string response = coordinator->executeCommand(start_cmd);
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("status"), std::string::npos);
    
    // Comando de status
    IPCCommand status_cmd;
    status_cmd.action = "status";
    status_cmd.mechanism = IPCMechanism::PIPES;
    
    response = coordinator->executeCommand(status_cmd);
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("mechanisms"), std::string::npos);
}

// Teste de envio de mensagens
TEST_F(IPCCoordinatorTest, MessageSending) {
    ASSERT_TRUE(coordinator->initialize());
    
    // Inicia shared memory primeiro
    ASSERT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    
    // Envia mensagem
    std::string test_msg = "Teste de mensagem do coordinator";
    bool sent = coordinator->sendMessage(IPCMechanism::SHARED_MEMORY, test_msg);
    EXPECT_TRUE(sent);
    
    // Tenta enviar para mecanismo não iniciado
    sent = coordinator->sendMessage(IPCMechanism::PIPES, test_msg);
    EXPECT_FALSE(sent);
}

// Teste de restart de mecanismo
TEST_F(IPCCoordinatorTest, MechanismRestart) {
    ASSERT_TRUE(coordinator->initialize());
    
    // Inicia, para e reinicia
    EXPECT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    EXPECT_TRUE(coordinator->stopMechanism(IPCMechanism::SHARED_MEMORY));
    EXPECT_TRUE(coordinator->restartMechanism(IPCMechanism::SHARED_MEMORY));
    
    auto status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_TRUE(status.is_active);
}

// Teste de logs
TEST_F(IPCCoordinatorTest, LogRetrieval) {
    ASSERT_TRUE(coordinator->initialize());
    
    // Inicia um mecanismo pra gerar logs
    coordinator->startMechanism(IPCMechanism::SHARED_MEMORY);
    coordinator->sendMessage(IPCMechanism::SHARED_MEMORY, "test message");
    
    auto logs = coordinator->getLogs(IPCMechanism::SHARED_MEMORY, 10);
    EXPECT_GT(logs.size(), 0);
}

// Teste de conversão string->mechanism
TEST_F(IPCCoordinatorTest, StringConversion) {
    // Testa mechanismToString via status
    auto status = coordinator->getMechanismStatus(IPCMechanism::PIPES);
    EXPECT_EQ(status.name, "pipes");
    
    status = coordinator->getMechanismStatus(IPCMechanism::SOCKETS);
    EXPECT_EQ(status.name, "sockets");
    
    status = coordinator->getMechanismStatus(IPCMechanism::SHARED_MEMORY);
    EXPECT_EQ(status.name, "shared_memory");
}

// Teste de comando inválido
TEST_F(IPCCoordinatorTest, InvalidCommand) {
    ASSERT_TRUE(coordinator->initialize());
    
    IPCCommand invalid_cmd;
    invalid_cmd.action = "invalid_action";
    invalid_cmd.mechanism = IPCMechanism::PIPES;
    
    std::string response = coordinator->executeCommand(invalid_cmd);
    EXPECT_FALSE(response.empty());
    EXPECT_NE(response.find("error"), std::string::npos);
}

// Teste de parsing de comando JSON
TEST_F(IPCCoordinatorTest, JSONCommandParsing) {
    IPCCommand cmd;
    
    // Teste parsing de start command
    std::string json = R"({"action":"start","mechanism":"pipes","message":"test"})";
    EXPECT_TRUE(cmd.fromJSON(json));
    EXPECT_EQ(cmd.action, "start");
    EXPECT_EQ(cmd.mechanism, IPCMechanism::PIPES);
    EXPECT_EQ(cmd.message, "test");
    
    // Teste parsing de send command
    json = R"({"action":"send","mechanism":"shared_memory","message":"hello world"})";
    EXPECT_TRUE(cmd.fromJSON(json));
    EXPECT_EQ(cmd.action, "send");
    EXPECT_EQ(cmd.mechanism, IPCMechanism::SHARED_MEMORY);
    EXPECT_EQ(cmd.message, "hello world");
}

// Teste de timestamp
TEST_F(IPCCoordinatorTest, TimestampGeneration) {
    std::string timestamp = coordinator->getCurrentTimestamp();
    EXPECT_FALSE(timestamp.empty());
    EXPECT_GT(timestamp.length(), 10); // timestamp deve ter pelo menos 10 chars
}