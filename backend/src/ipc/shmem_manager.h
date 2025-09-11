/**
 * @file shmem_manager.h
 * @brief Gerenciador de memória compartilhada para comunicação IPC entre processos
 * 
 * Este arquivo implementa comunicação via memória compartilhada (System V IPC).
 * É o mecanismo IPC MAIS RÁPIDO para transferir grandes volumes de dados,
 * pois os processos acessam diretamente a mesma região de memória física.
 * 
 * VANTAGENS DA MEMÓRIA COMPARTILHADA:
 * - MAIS RÁPIDA: acesso direto à memória, sem cópias
 * - GRANDE CAPACIDADE: pode compartilhar megabytes de dados
 * - PERSISTENTE: dados permanecem mesmo se processos morrem
 * - MÚLTIPLOS ACESSOS: vários processos podem acessar simultaneamente
 * 
 * DESVANTAGENS:
 * - COMPLEXA: requer sincronização manual com semáforos
 * - SEM PROTEÇÃO: não há controle automático de concorrência
 * - SISTEMA V IPC: APIs mais antigas e complexas
 */

#pragma once  // Garante inclusão única

#include <string>        // Para manipulação de strings
#include <vector>        // Para arrays dinâmicos
#include <sys/ipc.h>     // Para constantes IPC (IPC_PRIVATE, etc)
#include <sys/shm.h>     // Para shmget(), shmat(), shmctl() - memória compartilhada
#include <sys/sem.h>     // Para semget(), semop() - semáforos para sincronização
#include <unistd.h>      // Para fork(), getpid()
#include <sys/wait.h>    // Para waitpid() - esperar processo filho
#include "../common/logger.h"  // Sistema de logging

namespace ipc_project {

/**
 * Estrutura que armazena informações da memória compartilhada para exibição no frontend
 * 
 * Esta estrutura contém todas as informações necessárias sobre operações e status
 * da memória compartilhada que precisam ser enviadas para o dashboard web.
 * 
 * É usada para monitoramento em tempo real de:
 * - Conteúdo atual da memória
 * - Estado de sincronização (quem tem acesso)
 * - Processos em espera
 * - Estatísticas de performance
 */
struct SharedMemoryData {
    std::string content;                    // Conteúdo atual armazenado na memória compartilhada
    size_t size;                           // Tamanho do segmento de memória em bytes
    std::string sync_state;                // Estado atual de sincronização: "locked" ou "unlocked"
    std::vector<pid_t> waiting_processes;  // Lista de PIDs de processos esperando acesso
    std::string last_modified;             // Timestamp ISO da última modificação
    std::string operation;                 // Última operação realizada: "create", "write", "read", "destroy"
    pid_t process_id;                      // PID do processo que realizou a operação
    std::string status;                    // Status do resultado: "success" ou "error"
    std::string error_message;             // Mensagem de erro legível se a operação falhou
    double time_ms;                        // Tempo gasto na operação em milissegundos
    
    std::string toJSON() const;            // Serializa estes dados para formato JSON
    std::string getCurrentTimestamp() const; // Obtém tempo atual em formato ISO
};

/**
 * Estrutura interna que reside no segmento de memória compartilhada
 * 
 * Esta é a estrutura de dados real que fica mapeada na memória compartilhada
 * e é acessível por todos os processos. Contém tanto os dados do usuário
 * quanto metadados de sincronização.
 * 
 * LAYOUT DA MEMÓRIA:
 * [dados do usuário][metadados de controle][informações de sincronização]
 * 
 * Esta estrutura fica fisicamente na memória compartilhada, por isso
 * todos os processos vêem exatamente a mesma cópia.
 */
struct SharedMemorySegment {
    char data[1024];                       // Armazenamento dos dados do usuário (string terminada por null)
    pid_t last_writer;                     // PID do processo que escreveu por último
    time_t last_modified;                  // Timestamp Unix da última modificação
    int reader_count;                      // Número de processos atualmente lendo
    bool is_writing;                       // true se um escritor possui o lock atualmente
};

/**
 * Gerenciador de memória compartilhada de alto nível com sincronização leitor-escritor
 * 
 * Esta classe implementa um sistema robusto de memória compartilhada usando primitivas
 * System V IPC (shmget, shmat) combinado com semáforos para padrões de acesso
 * leitor-escritor thread-safe. Trata automaticamente:
 * 
 * - Criação e anexação de segmentos de memória
 * - Sincronização baseada em semáforos (problema leitores-escritores)
 * - Limpeza de processos e gerenciamento de recursos
 * - Tratamento e recuperação de erros
 * - Comunicação entre processos
 * 
 * PROBLEMA CLÁSSICO LEITORES-ESCRITORES:
 * A sincronização segue o padrão clássico leitores-escritores:
 * - MÚLTIPLOS LEITORES podem acessar simultaneamente
 * - ESCRITORES obtêm acesso EXCLUSIVO
 * - Leitores são BLOQUEADOS enquanto um escritor está ativo
 * - Escritores são BLOQUEADOS enquanto há leitores ativos
 * 
 * VANTAGENS DESTA IMPLEMENTAÇÃO:
 * - Sem starvation: escritores eventualment conseguem acesso
 * - Performance: múltiplos leitores simultâneos
 * - Robustez: limpeza automática de recursos
 */
class SharedMemoryManager {
public:
    SharedMemoryManager();   // Construtor - inicializa variáveis
    ~SharedMemoryManager();  // Destrutor - limpa recursos automaticamente

    // ========== Operações Básicas de Memória Compartilhada ==========
    bool createSharedMemory(key_t key = IPC_PRIVATE);  // Cria novo segmento de memória
    bool attachToMemory(key_t key);                    // Anexa a segmento existente
    bool writeMessage(const std::string& message);     // Escreve mensagem na memória
    std::string readMessage();                         // Lê mensagem da memória
    void destroySharedMemory();                        // Remove segmento completamente
    
    // ========== Operações de Sincronização (Leitores-Escritores) ==========
    bool lockForWrite();                               // Lock exclusivo para escrita
    bool lockForRead();                                // Lock compartilhado para leitura
    bool unlock();                                     // Libera qualquer lock
    
    // ========== Monitoramento e Status ==========
    SharedMemoryData getLastOperation() const;         // Dados da última operação
    void printJSON() const;                            // Imprime JSON no stdout
    bool isActive() const;                             // Se está ativo
    key_t getKey() const;                              // Chave do segmento
    
    // ========== Operações Multi-Processo ==========
    bool forkAndTest();                                // Cria processo filho para testes
    bool isParent() const;                             // Se este é o processo pai
    void waitForChild();                               // Espera processo filho terminar

private:
    // ========== Identificadores System V IPC ==========
    int shmid_;                            // ID do segmento de memória compartilhada
    int semid_;                            // ID do conjunto de semáforos
    SharedMemorySegment* shared_segment_;  // Ponteiro para segmento mapeado
    key_t shm_key_;                        // Chave da memória compartilhada
    
    // ========== Estado do Processo ==========
    bool is_creator_;                      // Se este processo criou o segmento
    bool is_attached_;                     // Se está anexado ao segmento
    bool is_parent_;                       // Se este é o processo pai
    pid_t child_pid_;                      // PID do processo filho
    
    // ========== Monitoramento ==========
    SharedMemoryData last_operation_;      // Dados da última operação
    Logger& logger_;                       // Logger para debug
    
    /**
     * @brief Semaphore indices for the semaphore set
     * 
     * We use a set of 3 semaphores to implement the readers-writers synchronization:
     * - SEM_MUTEX: General purpose mutex (currently unused but reserved)
     * - SEM_READER_MUTEX: Protects the reader_count variable
     * - SEM_WRITE: Controls exclusive access for writers and blocks new readers when a writer is waiting
     */
    static const int SEM_MUTEX = 0;       // General mutex semaphore (reserved)
    static const int SEM_READER_MUTEX = 1; // Protects reader_count modifications
    static const int SEM_WRITE = 2;        // Exclusive write access control
    static const int SEM_COUNT = 3;        // Total number of semaphores in the set
    
    // Helper operations
    bool createSemaphores();               // Create semaphore set
    bool attachToSemaphores();             // Attach to existing semaphores
    bool semaphoreOp(int sem_num, int op); // Generic semaphore operation
    void semaphoreWait(int sem_num);       // P(semaphore) - decrement
    void semaphoreSignal(int sem_num);     // V(semaphore) - increment
    
    double getCurrentTimeMs() const;       // Get current time in ms
    std::string getCurrentTimestamp() const; // Formatted timestamp
    void updateOperation(const std::string& op, const std::string& status, 
                        const std::string& error = "");
    
    // Cleanup
    void cleanup();                        // Clean up resources
};

} // namespace ipc_project