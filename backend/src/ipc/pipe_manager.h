/**
 * @file pipe_manager.h
 * @brief Gerenciador de pipes anonimos pra comunicacao IPC entre processos
 */

#pragma once

#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include "../common/logger.h"

namespace ipc_project {

// Estrutura pra guardar dados do pipe e mandar pro frontend
struct PipeData {
    std::string message;        
    size_t bytes;              // quantos bytes foram enviados
    double time_ms;            // Tempo que demorou
    std::string status;        // se funcionou ou deu erro
    pid_t sender_pid;          
    pid_t receiver_pid;        
    
    std::string toJSON() const; // converte pra JSON
};

// Classe principal pra gerenciar pipes anonimos
// usa fork() pra criar processo pai e filho que conversam via pipes
class PipeManager {
public:
    PipeManager();  
    ~PipeManager(); 

    bool createPipe();       // Cria o pipe e faz o fork
    bool isParent() const;   
    
    // funcoes de comunicacao
    bool sendMessage(const std::string& message);    // manda mensagem (so o pai)
    std::string receiveMessage();                     // Recebe mensagem (so o filho)
    
    // pra monitorar no frontend
    PipeData getLastOperation() const;  
    void printJSON() const;             // imprime JSON no stdout
    
    void closePipe();      // fecha pipe e espera processo filho
    bool isActive() const;

private:
    int pipe_fd_[2];              // Descriptors do pipe [0]=leitura, [1]=escrita  
    pid_t child_pid_;             // PID do processo filho
    bool is_parent_;              // True se for o processo pai
    bool is_active_;              // se o pipe ta ativo
    
    PipeData last_operation_;     // dados da ultima operacao
    Logger& logger_;              // Logger pra debug
    
    // funcoes auxiliares
    double getCurrentTimeMs() const;  // Pega tempo atual em ms
    void updateOperation(const std::string& msg, size_t bytes, const std::string& status);
    void runChildLoop();              // Loop principal do processo filho
};

} // namespace ipc_project