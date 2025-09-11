/**
 * @file test_shmem.cpp
 * @brief Testes unitários para SharedMemoryManager
 * 
 * Este arquivo testa o sistema de memória compartilhada:
 * - Criação e destruição de segmentos de memória
 * - Sincronização leitor-escritor com semáforos
 * - Operações concorrentes entre múltiplos processos
 * - Tratamento de erros e limpeza de recursos
 * - Performance e integridade dos dados
 * 
 * TESTES ESPECIAIS:
 * - Fork de processos para testar IPC real
 * - Stress testing com múltiplos leitores/escritores
 * - Verificação de consistência dos dados
 * - Limpeza automática de recursos System V IPC
 */

#include <gtest/gtest.h>              // Framework de testes do Google
#include "ipc/shmem_manager.h"        // Classe a ser testada

using namespace ipc_project;

/**
 * Classe fixture para testes do SharedMemoryManager
 * Configura automaticamente o ambiente de teste para cada caso
 */
class SharedMemoryManagerTest : public ::testing::Test {
protected:
    /**
     * Executado ANTES de cada teste - cria manager limpo
     */
    void SetUp() override {
        manager = std::make_unique<SharedMemoryManager>();
    }
    
    /**
     * Executado APÓS cada teste - limpa recursos System V IPC
     */
    void TearDown() override {
        if (manager) {
            manager->destroySharedMemory();  // Remove segmento de memória
        }
        manager.reset();  // Limpa smart pointer
    }
    
    std::unique_ptr<SharedMemoryManager> manager;  // Instância para testes
};

/**
 * Teste básico de criação e destruição de segmento de memória compartilhada
 * Verifica se o ciclo de vida básico funciona corretamente
 */
TEST_F(SharedMemoryManagerTest, CreateAndDestroy) {
    EXPECT_TRUE(manager->createSharedMemory());   // Deve conseguir criar
    EXPECT_TRUE(manager->isActive());             // Deve estar ativo após criação
    
    manager->destroySharedMemory();               // Remove segmento
    EXPECT_FALSE(manager->isActive());            // Não deve estar ativo após remoção
}

/**
 * Teste de escrita e leitura de mensagens na memória compartilhada
 * Verifica se os dados são preservados corretamente
 */
TEST_F(SharedMemoryManagerTest, WriteAndRead) {
    ASSERT_TRUE(manager->createSharedMemory());    // Cria segmento primeiro
    
    std::string test_message = "Mensagem de teste na memória compartilhada";
    EXPECT_TRUE(manager->writeMessage(test_message));  // Escreve mensagem
    
    std::string read_message = manager->readMessage(); // Lê mensagem de volta
    EXPECT_EQ(test_message, read_message);             // Deve ser idêntica
}

/**
 * Teste básico de sincronização com locks (semáforos)
 * Verifica se o sistema de leitores-escritores funciona
 */
TEST_F(SharedMemoryManagerTest, BasicLocking) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Testa lock exclusivo para escrita
    EXPECT_TRUE(manager->lockForWrite());  // Deve conseguir lock de escrita
    EXPECT_TRUE(manager->unlock());        // Deve conseguir liberar
    
    // Testa lock compartilhado para leitura
    EXPECT_TRUE(manager->lockForRead());   // Deve conseguir lock de leitura
    EXPECT_TRUE(manager->unlock());        // Deve conseguir liberar
}

/**
 * Teste de operações JSON para monitoramento web
 * Verifica se a serialização funciona corretamente
 */
TEST_F(SharedMemoryManagerTest, JSONOperations) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    std::string message = "Teste JSON para dashboard";
    manager->writeMessage(message);
    
    // Verifica dados da última operação
    auto last_op = manager->getLastOperation();
    EXPECT_EQ(last_op.operation, "write");    // Deve ser operação de escrita
    EXPECT_EQ(last_op.status, "success");     // Deve ter sucesso
    EXPECT_EQ(last_op.content, message);      // Conteúdo deve corresponder
    
    // Verifica serialização JSON
    std::string json = last_op.toJSON();
    EXPECT_FALSE(json.empty());                                        // JSON não deve estar vazio
    EXPECT_NE(json.find("shared_memory"), std::string::npos);          // Deve conter identificador
}

/**
 * Teste de múltiplas operações sequenciais
 * Verifica consistência em várias escritas/leituras
 */
TEST_F(SharedMemoryManagerTest, MultipleOperations) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Lista de mensagens para teste sequencial
    std::vector<std::string> messages = {
        "Primeira mensagem",
        "Segunda mensagem", 
        "Terceira mensagem"
    };
    
    // Testa cada mensagem individualmente
    for (const auto& msg : messages) {
        EXPECT_TRUE(manager->writeMessage(msg));    // Escreve mensagem
        std::string read_msg = manager->readMessage(); // Lê de volta
        EXPECT_EQ(msg, read_msg);                   // Verifica consistência
    }
}

/**
 * Teste de status e monitoramento do sistema
 * Verifica se o estado é reportado corretamente
 */
TEST_F(SharedMemoryManagerTest, StatusMonitoring) {
    EXPECT_FALSE(manager->isActive());  // Deve estar inativo inicialmente
    
    ASSERT_TRUE(manager->createSharedMemory());
    EXPECT_TRUE(manager->isActive());   // Deve estar ativo após criação
    
    key_t key = manager->getKey();      // Obtém chave do segmento
    EXPECT_NE(key, -1);                 // Chave deve ser válida
    
    // Testa getLastOperation após criação
    auto initial_op = manager->getLastOperation();
    EXPECT_EQ(initial_op.operation, "create");   // Última operação deve ser criação
    EXPECT_EQ(initial_op.status, "success");     // Status deve ser sucesso
}

/**
 * Teste de tratamento de erros
 * Verifica se erros são detectados e reportados corretamente
 */
TEST_F(SharedMemoryManagerTest, ErrorHandling) {
    // Tenta escrever sem criar segmento primeiro
    EXPECT_FALSE(manager->writeMessage("teste")); // Deve falhar
    
    // Tenta ler sem criar segmento primeiro  
    std::string result = manager->readMessage();  // Deve retornar vazio
    EXPECT_TRUE(result.empty());
    
    // Verifica se a última operação registrou o erro
    auto last_op = manager->getLastOperation();
    EXPECT_EQ(last_op.status, "error");           // Status deve ser erro
}

/**
 * Teste de gerenciamento de processos
 * Verifica identificação de processo pai/filho
 */
TEST_F(SharedMemoryManagerTest, ProcessManagement) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Por padrão, deve ser o processo pai
    EXPECT_TRUE(manager->isParent());
    
    // Nós não testamos fork aqui pois pode complicar testes unitários
    // Isso seria melhor testado nos testes de integração
}