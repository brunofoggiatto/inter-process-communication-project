/**
 * @file ipc_coordinator.cpp
 * @brief Implementação do coordenador central para todos os mecanismos IPC
 */

#include "ipc_coordinator.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <csignal>

namespace ipc_project {

// Instância estática pro signal handler
IPCCoordinator* IPCCoordinator::instance_ = nullptr;

// Implementação das estruturas

std::string MechanismStatus::toJSON() const {
    std::stringstream json;
    json << "{"
         << "\"type\":\"" << static_cast<int>(type) << "\","
         << "\"name\":\"" << name << "\","
         << "\"is_active\":" << (is_active ? "true" : "false") << ","
         << "\"is_running\":" << (is_running ? "true" : "false") << ","
         << "\"process_pid\":" << process_pid << ","
         << "\"last_error\":\"" << last_error << "\","
         << "\"last_operation\":\"" << last_operation << "\","
         << "\"uptime_ms\":" << uptime_ms << ","
         << "\"messages_sent\":" << messages_sent << ","
         << "\"messages_received\":" << messages_received
         << "}";
    return json.str();
}

std::string CoordinatorStatus::toJSON() const {
    std::stringstream json;
    json << "{"
         << "\"mechanisms\":[";
    
    for (size_t i = 0; i < mechanisms.size(); ++i) {
        json << mechanisms[i].toJSON();
        if (i < mechanisms.size() - 1) json << ",";
    }
    
    json << "],"
         << "\"all_active\":" << (all_active ? "true" : "false") << ","
         << "\"total_processes\":" << total_processes << ","
         << "\"startup_time\":\"" << startup_time << "\","
         << "\"total_uptime_ms\":" << total_uptime_ms << ","
         << "\"status\":\"" << status << "\""
         << "}";
    return json.str();
}

bool IPCCommand::fromJSON(const std::string& json) {
    // Implementação básica de parsing JSON
    // Em um projeto real, usaria biblioteca como nlohmann/json
    // Por simplicidade, vamos fazer parsing manual dos campos principais
    
    if (json.find("\"action\":\"start\"") != std::string::npos) {
        action = "start";
    } else if (json.find("\"action\":\"stop\"") != std::string::npos) {
        action = "stop";
    } else if (json.find("\"action\":\"send\"") != std::string::npos) {
        action = "send";
    } else if (json.find("\"action\":\"status\"") != std::string::npos) {
        action = "status";
    } else if (json.find("\"action\":\"logs\"") != std::string::npos) {
        action = "logs";
    } else {
        return false;
    }
    
    // Determina o mecanismo
    if (json.find("\"mechanism\":\"pipes\"") != std::string::npos) {
        mechanism = IPCMechanism::PIPES;
    } else if (json.find("\"mechanism\":\"sockets\"") != std::string::npos) {
        mechanism = IPCMechanism::SOCKETS;
    } else if (json.find("\"mechanism\":\"shared_memory\"") != std::string::npos) {
        mechanism = IPCMechanism::SHARED_MEMORY;
    } else {
        mechanism = IPCMechanism::PIPES; // padrão
    }
    
    // Extrai mensagem se houver
    size_t msg_start = json.find("\"message\":\"");
    if (msg_start != std::string::npos) {
        msg_start += 11; // tamanho de "message":"
        size_t msg_end = json.find("\"", msg_start);
        if (msg_end != std::string::npos) {
            message = json.substr(msg_start, msg_end - msg_start);
        }
    }
    
    return true;
}

std::string IPCCommand::toJSON() const {
    std::stringstream json;
    json << "{"
         << "\"action\":\"" << action << "\","
         << "\"mechanism\":\"" << static_cast<int>(mechanism) << "\","
         << "\"message\":\"" << message << "\""
         << "}";
    return json.str();
}

// Implementação da classe principal

IPCCoordinator::IPCCoordinator() 
    : is_running_(false)
    , shutdown_requested_(false)
    , startup_time_(getCurrentTimestamp())
    , logger_(Logger::getInstance()) {
    
    instance_ = this;
    
    // Inicializa status dos mecanismos como inativos
    mechanism_status_[IPCMechanism::PIPES] = false;
    mechanism_status_[IPCMechanism::SOCKETS] = false;
    mechanism_status_[IPCMechanism::SHARED_MEMORY] = false;
    
    // Inicializa contadores de mensagens
    message_counts_[IPCMechanism::PIPES] = 0;
    message_counts_[IPCMechanism::SOCKETS] = 0;
    message_counts_[IPCMechanism::SHARED_MEMORY] = 0;
    
    logger_.info("IPCCoordinator inicializado", "COORDINATOR");
}

IPCCoordinator::~IPCCoordinator() {
    shutdown();
    instance_ = nullptr;
}

bool IPCCoordinator::initialize() {
    logger_.info("Inicializando coordenador IPC...", "COORDINATOR");
    
    try {
        setupSignalHandlers();
        
        // Inicializa os managers
        pipe_manager_ = std::make_unique<PipeManager>();
        socket_manager_ = std::make_unique<SocketManager>(); 
        shmem_manager_ = std::make_unique<SharedMemoryManager>();
        
        logger_.info("Managers criados com sucesso", "COORDINATOR");
        
        is_running_ = true;
        startup_time_ = getCurrentTimestamp();
        
        logger_.info("Coordenador IPC inicializado com sucesso", "COORDINATOR");
        return true;
        
    } catch (const std::exception& e) {
        logger_.error("Erro ao inicializar coordenador: " + std::string(e.what()), "COORDINATOR");
        return false;
    }
}

void IPCCoordinator::shutdown() {
    if (!is_running_) return;
    
    logger_.info("Iniciando shutdown do coordenador...", "COORDINATOR");
    shutdown_requested_ = true;
    
    // Para todos os mecanismos
    stopMechanism(IPCMechanism::PIPES);
    stopMechanism(IPCMechanism::SOCKETS);
    stopMechanism(IPCMechanism::SHARED_MEMORY);
    
    // Espera threads de monitoramento terminarem
    for (auto& thread : monitoring_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    monitoring_threads_.clear();
    
    // Mata processos filhos se ainda estiverem vivos
    killAllChildren();
    
    cleanup();
    is_running_ = false;
    
    logger_.info("Coordenador desligado", "COORDINATOR");
}

bool IPCCoordinator::isRunning() const {
    return is_running_;
}

bool IPCCoordinator::startMechanism(IPCMechanism mechanism) {
    std::string mech_name = mechanismToString(mechanism);
    logger_.info("Iniciando mecanismo: " + mech_name, "COORDINATOR");
    
    try {
        bool success = false;
        
        switch (mechanism) {
            case IPCMechanism::PIPES:
                success = initializePipes();
                break;
            case IPCMechanism::SOCKETS:
                success = initializeSockets();
                break;
            case IPCMechanism::SHARED_MEMORY:
                success = initializeSharedMemory();
                break;
        }
        
        if (success) {
            mechanism_status_[mechanism] = true;
            logMechanismActivity(mechanism, "started");
            logger_.info(mech_name + " iniciado com sucesso", "COORDINATOR");
        } else {
            logger_.error("Falha ao iniciar " + mech_name, "COORDINATOR");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logger_.error("Exceção ao iniciar " + mech_name + ": " + e.what(), "COORDINATOR");
        return false;
    }
}

bool IPCCoordinator::stopMechanism(IPCMechanism mechanism) {
    std::string mech_name = mechanismToString(mechanism);
    logger_.info("Parando mecanismo: " + mech_name, "COORDINATOR");
    
    mechanism_status_[mechanism] = false;
    
    // Remove PID se existir
    auto pid_it = mechanism_pids_.find(mechanism);
    if (pid_it != mechanism_pids_.end()) {
        pid_t pid = pid_it->second;
        if (isProcessAlive(pid)) {
            kill(pid, SIGTERM);
            // Aguarda um pouco pro processo terminar graciosamente
            usleep(100000); // 100ms
            if (isProcessAlive(pid)) {
                kill(pid, SIGKILL); // força se não terminou
            }
        }
        mechanism_pids_.erase(pid_it);
    }
    
    logMechanismActivity(mechanism, "stopped");
    logger_.info(mech_name + " parado", "COORDINATOR");
    return true;
}

bool IPCCoordinator::restartMechanism(IPCMechanism mechanism) {
    logger_.info("Reiniciando mecanismo: " + mechanismToString(mechanism), "COORDINATOR");
    
    stopMechanism(mechanism);
    usleep(500000); // Aguarda 500ms
    return startMechanism(mechanism);
}

bool IPCCoordinator::sendMessage(IPCMechanism mechanism, const std::string& message) {
    if (!mechanism_status_[mechanism]) {
        logger_.warning("Tentativa de enviar mensagem em mecanismo inativo: " + mechanismToString(mechanism), "COORDINATOR");
        return false;
    }
    
    bool success = false;
    
    try {
        switch (mechanism) {
            case IPCMechanism::PIPES:
                if (pipe_manager_ && pipe_manager_->isActive()) {
                    success = pipe_manager_->sendMessage(message);
                }
                break;
            case IPCMechanism::SOCKETS:
                if (socket_manager_ && socket_manager_->isActive()) {
                    success = socket_manager_->sendMessage(message);
                }
                break;
            case IPCMechanism::SHARED_MEMORY:
                if (shmem_manager_ && shmem_manager_->isActive()) {
                    success = shmem_manager_->writeMessage(message);
                }
                break;
        }
        
        if (success) {
            message_counts_[mechanism]++;
            logMechanismActivity(mechanism, "message_sent: " + message);
        }
        
    } catch (const std::exception& e) {
        logger_.error("Erro ao enviar mensagem via " + mechanismToString(mechanism) + ": " + e.what(), "COORDINATOR");
    }
    
    return success;
}

std::string IPCCoordinator::receiveMessage(IPCMechanism mechanism) {
    if (!mechanism_status_[mechanism]) {
        return "";
    }
    
    std::string message;
    
    try {
        switch (mechanism) {
            case IPCMechanism::PIPES:
                if (pipe_manager_ && pipe_manager_->isActive()) {
                    message = pipe_manager_->receiveMessage();
                }
                break;
            case IPCMechanism::SOCKETS:
                if (socket_manager_ && socket_manager_->isActive()) {
                    message = socket_manager_->receiveMessage();
                }
                break;
            case IPCMechanism::SHARED_MEMORY:
                if (shmem_manager_ && shmem_manager_->isActive()) {
                    message = shmem_manager_->readMessage();
                }
                break;
        }
        
        if (!message.empty()) {
            logMechanismActivity(mechanism, "message_received: " + message);
        }
        
    } catch (const std::exception& e) {
        logger_.error("Erro ao receber mensagem via " + mechanismToString(mechanism) + ": " + e.what(), "COORDINATOR");
    }
    
    return message;
}

CoordinatorStatus IPCCoordinator::getFullStatus() const {
    CoordinatorStatus status;
    
    // Status de cada mecanismo
    for (const auto& mech : {IPCMechanism::PIPES, IPCMechanism::SOCKETS, IPCMechanism::SHARED_MEMORY}) {
        status.mechanisms.push_back(getMechanismStatus(mech));
    }
    
    // Status geral
    status.all_active = std::all_of(mechanism_status_.begin(), mechanism_status_.end(),
                                   [](const auto& pair) { return pair.second; });
    
    status.total_processes = mechanism_pids_.size();
    status.startup_time = startup_time_;
    status.total_uptime_ms = getCurrentTimeMs();
    status.status = is_running_ ? "running" : "stopped";
    
    return status;
}

MechanismStatus IPCCoordinator::getMechanismStatus(IPCMechanism mechanism) const {
    MechanismStatus status;
    status.type = mechanism;
    status.name = mechanismToString(mechanism);
    status.is_active = mechanism_status_.at(mechanism);
    
    auto pid_it = mechanism_pids_.find(mechanism);
    if (pid_it != mechanism_pids_.end()) {
        status.process_pid = pid_it->second;
        status.is_running = isProcessAlive(status.process_pid);
    } else {
        status.process_pid = 0;
        status.is_running = false;
    }
    
    status.uptime_ms = getCurrentTimeMs(); // simplificado
    status.messages_sent = message_counts_.at(mechanism);
    status.messages_received = 0; // simplificado por agora
    
    return status;
}

std::string IPCCoordinator::executeCommand(const IPCCommand& command) {
    logger_.info("Executando comando: " + command.action + " no " + mechanismToString(command.mechanism), "COORDINATOR");
    
    std::stringstream response;
    response << "{\"status\":\"";
    
    try {
        if (command.action == "start") {
            bool success = startMechanism(command.mechanism);
            response << (success ? "success" : "error");
            response << "\",\"message\":\"" << mechanismToString(command.mechanism) 
                     << (success ? " started" : " failed to start") << "\"}";
                     
        } else if (command.action == "stop") {
            bool success = stopMechanism(command.mechanism);
            response << (success ? "success" : "error");
            response << "\",\"message\":\"" << mechanismToString(command.mechanism) 
                     << (success ? " stopped" : " failed to stop") << "\"}";
                     
        } else if (command.action == "send") {
            bool success = sendMessage(command.mechanism, command.message);
            response << (success ? "success" : "error");
            response << "\",\"message\":\"" << (success ? "message sent" : "failed to send message") << "\"}";
            
        } else if (command.action == "status") {
            response.str(""); // limpa
            return getStatusJSON();
            
        } else if (command.action == "logs") {
            // Por simplicidade, retorna status por enquanto
            response.str("");
            return getStatusJSON();
            
        } else {
            response << "error\",\"message\":\"unknown command: " << command.action << "\"}";
        }
        
    } catch (const std::exception& e) {
        response.str("");
        response << "{\"status\":\"error\",\"message\":\"exception: " << e.what() << "\"}";
    }
    
    return response.str();
}

std::string IPCCoordinator::getStatusJSON() const {
    return getFullStatus().toJSON();
}

void IPCCoordinator::printStatus() const {
    std::cout << getStatusJSON() << std::endl;
}

void IPCCoordinator::signalHandler(int signal) {
    if (instance_) {
        instance_->logger_.info("Sinal recebido: " + std::to_string(signal), "COORDINATOR");
        instance_->shutdown_requested_ = true;
        instance_->shutdown();
    }
}

void IPCCoordinator::setupSignalHandlers() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    logger_.info("Signal handlers configurados", "COORDINATOR");
}

// Funções privadas auxiliares

std::string IPCCoordinator::mechanismToString(IPCMechanism mech) const {
    switch (mech) {
        case IPCMechanism::PIPES: return "pipes";
        case IPCMechanism::SOCKETS: return "sockets";
        case IPCMechanism::SHARED_MEMORY: return "shared_memory";
        default: return "unknown";
    }
}

IPCMechanism IPCCoordinator::stringToMechanism(const std::string& str) const {
    if (str == "pipes") return IPCMechanism::PIPES;
    if (str == "sockets") return IPCMechanism::SOCKETS;
    if (str == "shared_memory") return IPCMechanism::SHARED_MEMORY;
    return IPCMechanism::PIPES; // padrão
}

std::string IPCCoordinator::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

double IPCCoordinator::getCurrentTimeMs() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

bool IPCCoordinator::isProcessAlive(pid_t pid) const {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

void IPCCoordinator::killAllChildren() {
    for (const auto& pair : mechanism_pids_) {
        pid_t pid = pair.second;
        if (isProcessAlive(pid)) {
            logger_.info("Terminando processo: " + std::to_string(pid), "COORDINATOR");
            kill(pid, SIGTERM);
            usleep(100000); // 100ms
            if (isProcessAlive(pid)) {
                kill(pid, SIGKILL);
            }
        }
    }
    mechanism_pids_.clear();
}

bool IPCCoordinator::initializePipes() {
    if (!pipe_manager_) return false;
    
    bool success = pipe_manager_->createPipe();
    if (success && pipe_manager_->isParent()) {
        // Processo pai continua
        logger_.info("Pipe inicializado como processo pai", "PIPES");
        return true;
    } else if (success && !pipe_manager_->isParent()) {
        // Processo filho - entra em loop de escuta
        logger_.info("Pipe inicializado como processo filho", "PIPES");
        // O processo filho deve aguardar mensagens
        return true;
    }
    
    return false;
}

bool IPCCoordinator::initializeSockets() {
    if (!socket_manager_) return false;
    
    bool success = socket_manager_->createSocket();
    if (success && socket_manager_->isParent()) {
        // Processo pai continua
        logger_.info("Socket inicializado como processo pai", "SOCKETS");
        return true;
    } else if (success && !socket_manager_->isParent()) {
        // Processo filho - entra em loop de escuta (nunca retorna)
        logger_.info("Socket inicializado como processo filho", "SOCKETS");
        // O processo filho fica aqui esperando mensagens
        return true;
    }
    
    return false;
}

bool IPCCoordinator::initializeSharedMemory() {
    if (!shmem_manager_) return false;
    
    bool success = shmem_manager_->createSharedMemory();
    if (success) {
        logger_.info("Memória compartilhada inicializada", "SHARED_MEMORY");
        return true;
    }
    
    return false;
}

void IPCCoordinator::logMechanismActivity(IPCMechanism mechanism, const std::string& activity) {
    std::string timestamp = getCurrentTimestamp();
    std::string log_entry = "[" + timestamp + "] " + activity;
    
    mechanism_logs_[mechanism].push_back(log_entry);
    
    // Mantém apenas os últimos 1000 logs por mecanismo
    if (mechanism_logs_[mechanism].size() > 1000) {
        mechanism_logs_[mechanism].erase(mechanism_logs_[mechanism].begin());
    }
}

void IPCCoordinator::cleanup() {
    pipe_manager_.reset();
    socket_manager_.reset();
    shmem_manager_.reset();
    
    mechanism_logs_.clear();
    mechanism_pids_.clear();
    
    logger_.info("Cleanup concluído", "COORDINATOR");
}

void IPCCoordinator::waitForAllChildren() {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        logger_.info("Processo filho " + std::to_string(pid) + " terminou", "COORDINATOR");
        
        // Remove da lista de PIDs
        for (auto it = mechanism_pids_.begin(); it != mechanism_pids_.end(); ++it) {
            if (it->second == pid) {
                mechanism_pids_.erase(it);
                break;
            }
        }
    }
}

std::vector<std::string> IPCCoordinator::getLogs(IPCMechanism mechanism, size_t count) {
    auto& logs = mechanism_logs_[mechanism];
    size_t start = logs.size() > count ? logs.size() - count : 0;
    return std::vector<std::string>(logs.begin() + start, logs.end());
}

} // namespace ipc_project