/**
 * @file pipe_manager.h
 * @brief Gerenciador de pipes anônimos para comunicação IPC entre processos
 * 
 * Este arquivo implementa comunicação via pipes anônimos usando fork().
 * Pipes são unidirecionais e muito rápidos, ideais para comunicação pai->filho.
 * Funciona criando um processo filho que fica ouvindo mensagens do processo pai.
 */

#pragma once  // Garante inclusão única

#include <string>       // Para manipulação de strings
#include <unistd.h>     // Para pipe(), fork(), read(), write()
#include <sys/wait.h>   // Para waitpid() - esperar processo filho
#include "../common/logger.h"  // Sistema de logging

namespace ipc_project {

/**
 * Estrutura que armazena dados de uma operação com pipe
 * Usada para monitoramento e estatísticas no dashboard web
 */
struct PipeData {
    std::string message;        // Mensagem que foi enviada/recebida
    size_t bytes;              // Número de bytes transferidos
    double time_ms;            // Tempo que a operação demorou em milissegundos
    std::string status;        // Status da operação: "success", "error", etc
    pid_t sender_pid;          // PID do processo que enviou a mensagem
    pid_t receiver_pid;        // PID do processo que recebeu a mensagem
    
    std::string toJSON() const; // Converte os dados para formato JSON
};

/**
 * Classe principal para gerenciar pipes anônimos
 * 
 * COMO FUNCIONA:
 * 1. createPipe() cria um pipe e faz fork() para criar processo filho
 * 2. Processo PAI usa sendMessage() para enviar dados
 * 3. Processo FILHO usa receiveMessage() para receber dados
 * 4. Comunicação é UNIDIRECIONAL: Pai -> Filho apenas
 * 
 * ARQUITETURA:
 * Processo Pai [write] ---> PIPE ---> [read] Processo Filho
 */
class PipeManager {
public:
    PipeManager();   // Construtor - inicializa variáveis
    ~PipeManager();  // Destrutor - fecha pipe e limpa recursos

    // ========== Inicialização ==========
    bool createPipe();       // Cria o pipe e executa fork() para criar processo filho
    bool isParent() const;   // Verifica se este processo é o pai (true) ou filho (false)
    
    // ========== Comunicação ==========
    bool sendMessage(const std::string& message);    // Envia mensagem (apenas o processo PAI pode usar)
    std::string receiveMessage();                     // Recebe mensagem (apenas o processo FILHO pode usar)
    
    // ========== Monitoramento e Status ==========
    PipeData getLastOperation() const;  // Retorna dados da última operação (para dashboard)
    void printJSON() const;             // Imprime status em formato JSON no stdout
    
    // ========== Controle ==========
    void closePipe();      // Fecha o pipe e espera o processo filho terminar
    bool isActive() const; // Verifica se o pipe está ativo e funcionando

private:
    // ========== Descritores de Arquivo ==========
    int pipe_fd_[2];              // Array com descritores do pipe: [0]=leitura, [1]=escrita  
    
    // ========== Controle de Processos ==========
    pid_t child_pid_;             // PID do processo filho (0 se for o próprio filho)
    bool is_parent_;              // true = processo pai, false = processo filho
    bool is_active_;              // true se o pipe está ativo e funcionando
    
    // ========== Monitoramento ==========
    PipeData last_operation_;     // Dados da última operação realizada
    Logger& logger_;              // Referência ao sistema de logging
    
    // ========== Funções Auxiliares ==========
    double getCurrentTimeMs() const;  // Obtém o tempo atual em milissegundos
    void updateOperation(const std::string& msg, size_t bytes, const std::string& status); // Atualiza dados da operação
    void runChildLoop();              // Loop principal executado pelo processo filho
};

} // namespace ipc_project