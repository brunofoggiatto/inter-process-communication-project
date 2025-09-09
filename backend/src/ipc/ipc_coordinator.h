/**
 * @file ipc_coordinator.h
 * @brief Coordenador central para gerenciar todos os mecanismos IPC do projeto
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <thread>
#include <atomic>
#include <signal.h>
#include <sys/wait.h>
#include "pipe_manager.h"
#include "socket_manager.h"
#include "shmem_manager.h"
#include "../common/logger.h"

namespace ipc_project {

// Enum pra identificar os diferentes mecanismos IPC
enum class IPCMechanism {
    PIPES,
    SOCKETS, 
    SHARED_MEMORY
};

// Estrutura pra guardar status de um mecanismo específico
struct MechanismStatus {
    IPCMechanism type;
    std::string name;
    bool is_active;
    bool is_running;
    pid_t process_pid;      // PID do processo específico do mecanismo
    std::string last_error;
    std::string last_operation;
    double uptime_ms;
    size_t messages_sent;
    size_t messages_received;
    
    std::string toJSON() const;
};

// Estrutura geral de status do coordenador
struct CoordinatorStatus {
    std::vector<MechanismStatus> mechanisms;
    bool all_active;
    size_t total_processes;
    std::string startup_time;
    double total_uptime_ms;
    std::string status;          // "running", "starting", "stopping", "error"
    
    std::string toJSON() const;
};

// Estrutura pra comandos que vem do servidor HTTP
struct IPCCommand {
    std::string action;          // "start", "stop", "send", "status", "logs"
    IPCMechanism mechanism;      // qual mecanismo usar
    std::string message;         // mensagem pra enviar (se aplicável)
    std::map<std::string, std::string> parameters; // parâmetros extras
    
    bool fromJSON(const std::string& json);
    std::string toJSON() const;
};

// Classe principal que coordena todos os mecanismos IPC
// Responsável por inicializar, gerenciar e coordenar Pipes, Sockets e Shared Memory
class IPCCoordinator {
public:
    IPCCoordinator();
    ~IPCCoordinator();

    // Inicialização e shutdown
    bool initialize();                           // Inicializa todos os mecanismos
    void shutdown();                             // Para tudo de forma controlada
    bool isRunning() const;                      // Se o coordenador tá rodando
    
    // Controle individual dos mecanismos
    bool startMechanism(IPCMechanism mechanism); // Liga um mecanismo específico
    bool stopMechanism(IPCMechanism mechanism);  // Para um mecanismo específico
    bool restartMechanism(IPCMechanism mechanism); // Reinicia mecanismo
    
    // Envio de mensagens
    bool sendMessage(IPCMechanism mechanism, const std::string& message);
    std::string receiveMessage(IPCMechanism mechanism);
    
    // Status e monitoramento
    CoordinatorStatus getFullStatus() const;     // Status completo de tudo
    MechanismStatus getMechanismStatus(IPCMechanism mechanism) const;
    std::vector<std::string> getLogs(IPCMechanism mechanism, size_t count = 100);
    
    // Interface pro servidor HTTP
    std::string executeCommand(const IPCCommand& command);  // Executa comando e retorna JSON
    std::string getStatusJSON() const;           // Status em formato JSON
    std::string getMechanismDetailJSON(IPCMechanism mechanism) const; // Última operação + status
    void printStatus() const;                    // Imprime status no stdout
    
    // Gerenciamento de processos
    void waitForAllChildren();                   // Espera todos os processos filhos
    void killAllChildren();                      // Mata todos os processos filhos
    
    // Handler pra sinais (SIGINT, SIGTERM)
    static void signalHandler(int signal);
    void setupSignalHandlers();
    
    // Utility functions
    std::string getCurrentTimestamp() const;

private:
    // Managers dos mecanismos IPC
    std::unique_ptr<PipeManager> pipe_manager_;
    std::unique_ptr<SocketManager> socket_manager_;
    std::unique_ptr<SharedMemoryManager> shmem_manager_;
    
    // Controle de estado
    std::atomic<bool> is_running_;
    std::atomic<bool> shutdown_requested_;
    std::map<IPCMechanism, bool> mechanism_status_;
    std::map<IPCMechanism, pid_t> mechanism_pids_;
    
    // Threads pra monitoramento contínuo
    std::vector<std::thread> monitoring_threads_;
    
    // Dados de status
    std::string startup_time_;
    std::map<IPCMechanism, std::vector<std::string>> mechanism_logs_;
    std::map<IPCMechanism, size_t> message_counts_;
    
    Logger& logger_;
    
    // Instância estática pro signal handler
    static IPCCoordinator* instance_;
    
    // Funções auxiliares
    std::string mechanismToString(IPCMechanism mech) const;
    IPCMechanism stringToMechanism(const std::string& str) const;
    double getCurrentTimeMs() const;
    
    // Monitoramento de processos
    bool isProcessAlive(pid_t pid) const;           // Verifica se processo tá vivo
    
    // Inicialização individual
    bool initializePipes();
    bool initializeSockets(); 
    bool initializeSharedMemory();
    
    // Cleanup
    void cleanup();
    void logMechanismActivity(IPCMechanism mechanism, const std::string& activity);
};

} // namespace ipc_project
