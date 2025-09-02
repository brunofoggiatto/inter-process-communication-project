/**
 * @file shmem_manager.h
 * @brief Gerenciador de memória compartilhada para comunicação IPC entre processos
 */

#pragma once

#include <string>
#include <vector>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../common/logger.h"

namespace ipc_project {

// Estrutura pra guardar dados da memória compartilhada e mandar pro frontend
struct SharedMemoryData {
    std::string content;                    // conteúdo atual da memória
    size_t size;                           // tamanho do segmento
    std::string sync_state;                // "locked" ou "unlocked" 
    std::vector<pid_t> waiting_processes;  // processos esperando acesso
    std::string last_modified;             // timestamp da última modificação
    std::string operation;                 // "create", "write", "read", "destroy"
    pid_t process_id;                      // PID do processo atual
    std::string status;                    // "success" ou "error"
    std::string error_message;             // mensagem de erro se houver
    double time_ms;                        // tempo da operação em ms
    
    std::string toJSON() const;            // converte pra JSON
    std::string getCurrentTimestamp() const; // timestamp formatado
};

// Estrutura interna que fica na memória compartilhada
struct SharedMemorySegment {
    char data[1024];                       // dados propriamente ditos
    pid_t last_writer;                     // último processo que escreveu
    time_t last_modified;                  // timestamp da última modificação
    int reader_count;                      // quantos processos estão lendo
    bool is_writing;                       // se alguém tá escrevendo agora
};

// Classe principal pra gerenciar memória compartilhada
// Usa System V IPC (shmget, shmat) com semáforos pra sincronização
class SharedMemoryManager {
public:
    SharedMemoryManager();
    ~SharedMemoryManager();

    // Operações básicas de memória compartilhada
    bool createSharedMemory(key_t key = IPC_PRIVATE);  // Cria segmento
    bool attachToMemory(key_t key);                    // Anexa ao segmento existente
    bool writeMessage(const std::string& message);     // Escreve na memória
    std::string readMessage();                         // Lê da memória
    void destroySharedMemory();                        // Remove o segmento
    
    // Operações de sincronização
    bool lockForWrite();                               // Trava pra escrita exclusiva
    bool lockForRead();                                // Trava pra leitura compartilhada
    bool unlock();                                     // Libera o lock
    
    // Monitoramento e status
    SharedMemoryData getLastOperation() const;         // Dados da última operação
    void printJSON() const;                            // Imprime JSON no stdout
    bool isActive() const;                             // Se tá ativo
    key_t getKey() const;                              // Chave do segmento
    
    // Operações com múltiplos processos
    bool forkAndTest();                                // Cria processo filho pra testar
    bool isParent() const;                             // Se é o processo pai
    void waitForChild();                               // Espera processo filho terminar

private:
    int shmid_;                            // ID do segmento de memória compartilhada
    int semid_;                            // ID do conjunto de semáforos
    SharedMemorySegment* shared_segment_;  // Ponteiro pro segmento mapeado
    key_t shm_key_;                        // Chave da memória compartilhada
    bool is_creator_;                      // Se este processo criou o segmento
    bool is_attached_;                     // Se tá anexado ao segmento
    bool is_parent_;                       // Se é o processo pai
    pid_t child_pid_;                      // PID do processo filho
    
    SharedMemoryData last_operation_;      // Dados da última operação
    Logger& logger_;                       // Logger pra debug
    
    // Semáforos: [0] = mutex, [1] = reader_count_mutex, [2] = write_lock
    static const int SEM_MUTEX = 0;       // Semáforo mutex geral
    static const int SEM_READER_MUTEX = 1; // Mutex pra contador de leitores
    static const int SEM_WRITE = 2;        // Semáforo pra escrita
    static const int SEM_COUNT = 3;        // Total de semáforos
    
    // Operações auxiliares
    bool createSemaphores();               // Cria conjunto de semáforos
    bool semaphoreOp(int sem_num, int op); // Operação genérica em semáforo
    void semaphoreWait(int sem_num);       // P(semáforo) - decrementa
    void semaphoreSignal(int sem_num);     // V(semáforo) - incrementa
    
    double getCurrentTimeMs() const;       // Pega tempo atual em ms
    std::string getCurrentTimestamp() const; // Timestamp formatado
    void updateOperation(const std::string& op, const std::string& status, 
                        const std::string& error = "");
    
    // Limpeza
    void cleanup();                        // Limpa recursos
};

} // namespace ipc_project