/**
 * @file test_shmem.cpp
 * @brief Testes unitários para SharedMemoryManager
 */

#include <gtest/gtest.h>
#include "ipc/shmem_manager.h"

using namespace ipc_project;

class SharedMemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<SharedMemoryManager>();
    }
    
    void TearDown() override {
        if (manager) {
            manager->destroySharedMemory();
        }
        manager.reset();
    }
    
    std::unique_ptr<SharedMemoryManager> manager;
};

// Teste básico de criação e destruição
TEST_F(SharedMemoryManagerTest, CreateAndDestroy) {
    EXPECT_TRUE(manager->createSharedMemory());
    EXPECT_TRUE(manager->isActive());
    
    manager->destroySharedMemory();
    EXPECT_FALSE(manager->isActive());
}

// Teste de escrita e leitura
TEST_F(SharedMemoryManagerTest, WriteAndRead) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    std::string test_message = "Teste de mensagem na memória compartilhada";
    EXPECT_TRUE(manager->writeMessage(test_message));
    
    std::string read_message = manager->readMessage();
    EXPECT_EQ(test_message, read_message);
}

// Teste de sincronização básica
TEST_F(SharedMemoryManagerTest, BasicLocking) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Teste de lock para escrita
    EXPECT_TRUE(manager->lockForWrite());
    EXPECT_TRUE(manager->unlock());
    
    // Teste de lock para leitura
    EXPECT_TRUE(manager->lockForRead());
    EXPECT_TRUE(manager->unlock());
}

// Teste de operações JSON
TEST_F(SharedMemoryManagerTest, JSONOperations) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    std::string message = "Test JSON";
    manager->writeMessage(message);
    
    auto last_op = manager->getLastOperation();
    EXPECT_EQ(last_op.operation, "write");
    EXPECT_EQ(last_op.status, "success");
    EXPECT_EQ(last_op.content, message);
    
    std::string json = last_op.toJSON();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("shared_memory"), std::string::npos);
}

// Teste de múltiplas operações
TEST_F(SharedMemoryManagerTest, MultipleOperations) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Escreve várias mensagens
    std::vector<std::string> messages = {
        "Primeira mensagem",
        "Segunda mensagem", 
        "Terceira mensagem"
    };
    
    for (const auto& msg : messages) {
        EXPECT_TRUE(manager->writeMessage(msg));
        std::string read_msg = manager->readMessage();
        EXPECT_EQ(msg, read_msg);
    }
}

// Teste de status e monitoramento
TEST_F(SharedMemoryManagerTest, StatusMonitoring) {
    EXPECT_FALSE(manager->isActive());
    
    ASSERT_TRUE(manager->createSharedMemory());
    EXPECT_TRUE(manager->isActive());
    
    key_t key = manager->getKey();
    EXPECT_NE(key, -1);
    
    // Testa getLastOperation antes de qualquer operação
    auto initial_op = manager->getLastOperation();
    EXPECT_EQ(initial_op.operation, "create");
    EXPECT_EQ(initial_op.status, "success");
}

// Teste de erro - tentativa de usar sem criar
TEST_F(SharedMemoryManagerTest, ErrorHandling) {
    // Tenta escrever sem criar primeiro
    EXPECT_FALSE(manager->writeMessage("test"));
    
    // Tenta ler sem criar primeiro  
    std::string result = manager->readMessage();
    EXPECT_TRUE(result.empty());
    
    // Verifica que última operação tem erro
    auto last_op = manager->getLastOperation();
    EXPECT_EQ(last_op.status, "error");
}

// Teste de processo filho (se aplicável)
TEST_F(SharedMemoryManagerTest, ProcessManagement) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Por padrão, deve ser o processo pai
    EXPECT_TRUE(manager->isParent());
    
    // Não testamos fork aqui pois pode complicar os testes unitários
    // Isso seria melhor testado nos testes de integração
}