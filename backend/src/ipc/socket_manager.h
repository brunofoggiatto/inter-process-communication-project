/**
 * @file socket_manager.h
 * @brief Gerenciador de Unix Domain Sockets para comunicação IPC bidirecional
 * 
 * Este arquivo implementa comunicação via Unix Domain Sockets (AF_UNIX).
 * Diferente de pipes, sockets são BIDIRECIONAIS e mais flexíveis.
 * Ideal para arquiteturas cliente-servidor onde ambos lados podem enviar/receber.
 * 
 * VANTAGENS DOS SOCKETS:
 * - Comunicação bidirecional (ambos lados podem enviar/receber)
 * - Suporte a múltiplos clientes (não implementado nesta versão)
 * - Mais flexível que pipes
 * - Protocolo confiável
 */

#pragma once  // Garante inclusão única

#include <string>        // Para manipulação de strings
#include <sys/socket.h>  // Para socketpair(), send(), recv()
#include <sys/un.h>      // Para estruturas de Unix Domain Socket
#include <unistd.h>      // Para fork(), close()
#include <sys/wait.h>    // Para waitpid() - esperar processo filho
#include "../common/logger.h"  // Sistema de logging

namespace ipc_project {

/**
 * Estrutura que armazena dados de uma operação com socket
 * Usada para monitoramento e estatísticas no dashboard web
 * Similar ao PipeData, mas para sockets
 */
struct SocketData {
    std::string message;        // Mensagem que foi enviada/recebida
    size_t bytes;              // Número de bytes transferidos
    double time_ms;            // Tempo que a operação demorou em milissegundos
    std::string status;        // Status da operação: "success", "error", etc
    pid_t sender_pid;          // PID do processo que enviou a mensagem
    pid_t receiver_pid;        // PID do processo que recebeu a mensagem

    std::string toJSON() const; // Converte os dados para formato JSON
};

/**
 * Classe principal para gerenciar Unix Domain Sockets
 * 
 * COMO FUNCIONA:
 * 1. createSocket() cria um socketpair() e faz fork() para criar processo filho
 * 2. Ambos os processos (PAI e FILHO) podem enviar/receber mensagens
 * 3. Comunicação é BIDIRECIONAL: Pai <-> Filho
 * 4. Usa socketpair() que cria dois endpoints conectados
 * 
 * ARQUITETURA:
 * Processo Pai [socket_fd_[0]] <---> [socket_fd_[1]] Processo Filho
 * 
 * DIFERENÇAS DOS PIPES:
 * - Pipes: unidirecional, mais simples
 * - Sockets: bidirecional, mais flexível, suporte a protocolos
 */
class SocketManager {
public:
    SocketManager();   // Construtor - inicializa variáveis
    ~SocketManager();  // Destrutor - fecha socket e limpa recursos

    // ========== Inicialização ==========
    bool createSocket();        // Cria socketpair e executa fork() para criar processo filho
    bool isParent() const;      // Verifica se este processo é o pai (true) ou filho (false)

    // ========== Comunicação Bidirecional ==========
    bool sendMessage(const std::string& message);    // Envia mensagem (PAI ou FILHO podem usar)
    std::string receiveMessage();                     // Recebe mensagem (PAI ou FILHO podem usar)

    // ========== Monitoramento e Status ==========
    SocketData getLastOperation() const;  // Retorna dados da última operação (para dashboard)
    void printJSON() const;               // Imprime status em formato JSON no stdout

    // ========== Controle ==========
    void closeSocket();         // Fecha o socket e espera o processo filho terminar
    bool isActive() const;      // Verifica se o socket está ativo e funcionando

private:
    // ========== Descritores de Socket ==========
    int socket_fd_[2];           // Array com os dois endpoints do socketpair: [0] e [1]
                                 // Ambos podem ler/escrever (diferente de pipes)
    
    // ========== Controle de Processos ==========
    pid_t child_pid_;            // PID do processo filho (0 se for o próprio filho)
    bool is_parent_;             // true = processo pai, false = processo filho
    bool is_active_;             // true se o socket está ativo e funcionando

    // ========== Monitoramento ==========
    SocketData last_operation_;  // Dados da última operação realizada
    Logger& logger_;             // Referência ao sistema de logging

    // ========== Funções Auxiliares ==========
    double getCurrentTimeMs() const;  // Obtém o tempo atual em milissegundos
    void updateOperation(const std::string& msg, size_t bytes, const std::string& status); // Atualiza dados da operação
    void runChildLoop();              // Loop principal executado pelo processo filho
};

} // namespace ipc_project
