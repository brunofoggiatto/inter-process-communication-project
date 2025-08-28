/**
 * @file socket_manager.h
 * @brief Gerenciador de sockets locais para comunicação IPC entre processos
 */

#pragma once

#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../common/logger.h"

namespace ipc_project {

// Estrutura pra guardar dados do socket e mandar pro frontend
struct SocketData {
    std::string message;
    size_t bytes;
    double time_ms;
    std::string status;
    pid_t sender_pid;
    pid_t receiver_pid;

    std::string toJSON() const; // converte pra JSON
};

// Classe principal pra gerenciar sockets locais (AF_UNIX)
// Usa fork() pra criar processos que trocam mensagens via socket
class SocketManager {
public:
    SocketManager();
    ~SocketManager();

    bool createSocket();        // Cria socket e faz fork
    bool isParent() const;

    // Comunicação
    bool sendMessage(const std::string& message);    // Envia mensagem (pai)
    std::string receiveMessage();                    // Recebe mensagem (filho)

    // Monitoramento
    SocketData getLastOperation() const;
    void printJSON() const;

    void closeSocket();         // Fecha socket e espra processo filho
    bool isActive() const;

private:
    int socket_fd_[2];           // [0] e [1] são os dois extremos do socketpair
    pid_t child_pid_;
    bool is_parent_;
    bool is_active_;

    SocketData last_operation_;
    Logger& logger_;

    // Auxiliares
    double getCurrentTimeMs() const;
    void updateOperation(const std::string& msg, size_t bytes, const std::string& status);
};

} // namespace ipc_project
