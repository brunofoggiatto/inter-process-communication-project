/**
 * @file socket_manager.cpp
 * @brief Implementacao de sockets locais para comunicacao IPC
 */

#include "socket_manager.h"
#include <iostream>
#include <cstring>
#include <errno.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace ipc_project {

// Converte dados da operação atual pro formato JSON que o frontend entende
std::string SocketData::toJSON() const {
    std::ostringstream json;
    json << "{"
         << "\"message\":\"" << message << "\","
         << "\"bytes\":" << bytes << ","
         << "\"time_ms\":" << std::fixed << std::setprecision(3) << time_ms << ","
         << "\"status\":\"" << status << "\","
         << "\"sender_pid\":" << sender_pid << ","
         << "\"receiver_pid\":" << receiver_pid << ","
         << "\"ipc_type\":\"unix_socket\""
         << "}";
    return json.str();
}

// Construtor - inicializa o estado do socket manager
SocketManager::SocketManager()
    : is_parent_(true),
      is_active_(false),
      logger_(Logger::getInstance()) {

    socket_fd_[0] = -1;
    socket_fd_[1] = -1;
    child_pid_ = -1;

    // Inicializa estrutura da operação
    last_operation_.message = "";
    last_operation_.bytes = 0;
    last_operation_.time_ms = 0.0;
    last_operation_.status = "idle";
    last_operation_.sender_pid = getpid();
    last_operation_.receiver_pid = -1;

    logger_.info("SocketManager criado", "SOCKET");
}

// Destrutor - fecha socket e espera o processo filho
SocketManager::~SocketManager() {
    closeSocket();
    logger_.debug("SocketManager destruído", "SOCKET");
}

// Cria socket local com socketpair() e faz fork() nos processos
bool SocketManager::createSocket() {
    logger_.info("Criando socket local (AF_UNIX)", "SOCKET");

    auto start = std::chrono::high_resolution_clock::now();

    // socketpair cria dois FDs conectados diretamente
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fd_) == -1) {
        updateOperation("", 0, "error_create");
        logger_.error("Erro ao criar socketpair: " + std::string(strerror(errno)), "SOCKET");
        return false;
    }

    // Cria o processo filho
    child_pid_ = fork();

    if (child_pid_ == -1) {
        // erro no fork - limpa os sockets
        close(socket_fd_[0]);
        close(socket_fd_[1]);
        updateOperation("", 0, "error_fork");
        logger_.error("Erro ao fazer fork: " + std::string(strerror(errno)), "SOCKET");
        return false;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    if (child_pid_ == 0) {
        // Estamos no processo filho - só vai receber
        is_parent_ = false;
        close(socket_fd_[1]); // fecha lado do pai
        socket_fd_[1] = -1;

        last_operation_.sender_pid = getppid();
        last_operation_.receiver_pid = getpid();

        logger_.info("Processo filho iniciado", "SOCKET_CHILD");
    } else {
        // Processo pai - só vai enviar
        is_parent_ = true;
        close(socket_fd_[0]); // fecha lado do filho
        socket_fd_[0] = -1;

        last_operation_.sender_pid = getpid();
        last_operation_.receiver_pid = child_pid_;

        logger_.info("Processo pai com filho PID: " + std::to_string(child_pid_), "SOCKET");
    }

    is_active_ = true;
    updateOperation("socket_created", 0, "ready");
    last_operation_.time_ms = elapsed;

    return true;
}

bool SocketManager::isParent() const {
    return is_parent_;
}

// Função que o pai usa pra enviar mensagem ao filho
bool SocketManager::sendMessage(const std::string& message) {
    if (!is_active_ || !is_parent_ || socket_fd_[1] == -1) {
        updateOperation(message, 0, "error_invalid_state");
        logger_.error("Tentativa de envio inválida", "SOCKET");
        return false;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::string msg_final = message + "\n";  // facilita leitura do lado do filho

    ssize_t bytes_written = write(socket_fd_[1], msg_final.c_str(), msg_final.size());

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    if (bytes_written == -1) {
        updateOperation(message, 0, "error_write");
        logger_.error("Erro ao escrever no socket: " + std::string(strerror(errno)), "SOCKET");
        return false;
    }

    updateOperation(message, static_cast<size_t>(bytes_written), "sent");
    last_operation_.time_ms = elapsed;

    logger_.info("Mensagem enviada: '" + message + "' (" + std::to_string(bytes_written) + " bytes)", "SOCKET");

    printJSON(); // envia resultado pro frontend via stdout

    return true;
}

// Função que o filho usa pra receber mensagem do pai
std::string SocketManager::receiveMessage() {
    if (!is_active_ || is_parent_ || socket_fd_[0] == -1) {
        updateOperation("", 0, "error_invalid_state");
        logger_.error("Tentativa de leitura inválida", "SOCKET_CHILD");
        return "";
    }

    auto start = std::chrono::high_resolution_clock::now();

    char buf[1024];  // buffer temporário
    ssize_t bytes_read = read(socket_fd_[0], buf, sizeof(buf) - 1);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    if (bytes_read == -1) {
        updateOperation("", 0, "error_read");
        logger_.error("Erro ao ler do socket: " + std::string(strerror(errno)), "SOCKET_CHILD");
        return "";
    }

    if (bytes_read == 0) {
        // conexão fechada
        updateOperation("", 0, "eof");
        logger_.info("Socket fechado pelo pai (EOF)", "SOCKET_CHILD");
        return "";
    }

    buf[bytes_read] = '\0';
    std::string msg(buf);

    if (!msg.empty() && msg.back() == '\n') {
        msg.pop_back();  // remove newline
    }

    updateOperation(msg, static_cast<size_t>(bytes_read), "received");
    last_operation_.time_ms = elapsed;

    logger_.info("Mensagem recebida: '" + msg + "' (" + std::to_string(bytes_read) + " bytes)", "SOCKET_CHILD");

    printJSON(); // envia resultado pro frontend via stdout

    return msg;
}

SocketData SocketManager::getLastOperation() const {
    return last_operation_;
}

// imprime JSON da última operação pro stdout (pra o frontend)
void SocketManager::printJSON() const {
    std::cout << "SOCKET_JSON:" << last_operation_.toJSON() << std::endl;
    std::cout.flush();
}

// Fecha os sockets e finaliza o processo filho
void SocketManager::closeSocket() {
    if (!is_active_) return;

    logger_.info("Fechando socket", is_parent_ ? "SOCKET" : "SOCKET_CHILD");

    if (is_parent_) {
        // lado do pai
        if (socket_fd_[1] != -1) {
            close(socket_fd_[1]);
            socket_fd_[1] = -1;
        }

        if (child_pid_ > 0) {
            int status;
            logger_.debug("Esperando processo filho encerrar", "SOCKET");
            waitpid(child_pid_, &status, 0);

            if (WIFEXITED(status)) {
                logger_.info("Filho terminou com código: " + std::to_string(WEXITSTATUS(status)), "SOCKET");
            }
        }
    } else {
        // lado do filho
        if (socket_fd_[0] != -1) {
            close(socket_fd_[0]);
            socket_fd_[0] = -1;
        }
    }

    is_active_ = false;
    updateOperation("", 0, "closed");
}

bool SocketManager::isActive() const {
    return is_active_;
}

// pega tempo atual em milissegundos
double SocketManager::getCurrentTimeMs() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(epoch).count();
}

// Atualiza estrutura com dados da operação atual
void SocketManager::updateOperation(const std::string& msg, size_t bytes, const std::string& status) {
    last_operation_.message = msg;
    last_operation_.bytes = bytes;
    last_operation_.status = status;
    // tempo é preenchido na função chamadora
}

} // namespace ipc_project
