/**
 * @file ipc_coordinator.h
 * @brief Coordenador central para gerenciar todos os mecanismos IPC do projeto
 * 
 * Este arquivo define a classe principal que coordena todos os mecanismos de
 * comunicação inter-processos: pipes, sockets e memória compartilhada.
 * É o "cérebro" do sistema que gerencia, monitora e controla tudo.
 */

#pragma once  // Garante que o header seja incluído apenas uma vez

// Includes da biblioteca padrão C++
#include <string>        // Para manipulação de strings
#include <vector>        // Para arrays dinâmicos
#include <memory>        // Para smart pointers (unique_ptr, shared_ptr)
#include <map>           // Para mapeamentos chave-valor
#include <thread>        // Para threading
#include <atomic>        // Para variáveis thread-safe
#include <signal.h>      // Para tratamento de sinais do sistema
#include <sys/wait.h>    // Para esperar processos filhos

// Includes dos managers específicos de cada mecanismo IPC
#include "pipe_manager.h"    // Gerenciador de pipes anônimos
#include "socket_manager.h"  // Gerenciador de Unix Domain Sockets
#include "shmem_manager.h"   // Gerenciador de memória compartilhada
#include "../common/logger.h" // Sistema de logging

namespace ipc_project {

/**
 * Enumeração que identifica os diferentes mecanismos IPC disponíveis
 * Usado para especificar qual mecanismo usar nas operações
 */
enum class IPCMechanism {
    PIPES,          // Comunicação via pipes anônimos (unidirecional, rápido)
    SOCKETS,        // Comunicação via Unix Domain Sockets (bidirecional, flexível)
    SHARED_MEMORY   // Comunicação via memória compartilhada (mais rápido para dados grandes)
};

/**
 * Estrutura que armazena o status detalhado de um mecanismo IPC específico
 * Usada para monitoramento e exibição no dashboard web
 */
struct MechanismStatus {
    IPCMechanism type;           // Tipo do mecanismo (PIPES, SOCKETS, etc)
    std::string name;            // Nome legível do mecanismo
    bool is_active;              // Se o mecanismo está ativo/inicializado
    bool is_running;             // Se o mecanismo está rodando/processando
    pid_t process_pid;           // PID do processo específico do mecanismo
    std::string last_error;      // Último erro ocorrido (vazio se tudo OK)
    std::string last_operation;  // Descrição da última operação realizada
    double uptime_ms;            // Tempo de funcionamento em milissegundos
    size_t messages_sent;        // Contador de mensagens enviadas
    size_t messages_received;    // Contador de mensagens recebidas
    
    std::string toJSON() const;  // Converte para formato JSON para a API web
};

/**
 * Estrutura que representa o status geral do coordenador IPC
 * Agrupa informações de todos os mecanismos
 */
struct CoordinatorStatus {
    std::vector<MechanismStatus> mechanisms;  // Status de cada mecanismo individual
    bool all_active;              // true se todos os mecanismos estão ativos
    size_t total_processes;       // Número total de processos em execução
    std::string startup_time;     // Timestamp de quando o sistema foi iniciado
    double total_uptime_ms;       // Tempo total de funcionamento
    std::string status;           // Estado geral: "running", "starting", "stopping", "error"
    
    std::string toJSON() const;   // Converte para JSON para dashboard web
};

/**
 * Estrutura que representa comandos vindos do servidor HTTP
 * Usado para comunicação entre a interface web e o coordenador
 */
struct IPCCommand {
    std::string action;          // Ação: "start", "stop", "send", "status", "logs"
    IPCMechanism mechanism;      // Qual mecanismo IPC usar
    std::string message;         // Mensagem para enviar (quando aplicável)
    std::map<std::string, std::string> parameters; // Parâmetros adicionais
    
    bool fromJSON(const std::string& json);  // Deserializa de JSON
    std::string toJSON() const;              // Serializa para JSON
};

/**
 * Classe principal que coordena todos os mecanismos IPC do sistema
 * Esta é a classe "central" que gerencia pipes, sockets e memória compartilhada.
 * Funciona como um orquestrador que coordena tudo e fornece uma interface unificada.
 */
class IPCCoordinator {
public:
    IPCCoordinator();   // Construtor - inicializa variáveis internas
    ~IPCCoordinator();  // Destrutor - limpa recursos e para processos

    // ========== Inicialização e Controle de Vida Útil ==========
    bool initialize();                           // Inicializa todos os mecanismos IPC
    void shutdown();                             // Para tudo de forma controlada e limpa
    bool isRunning() const;                      // Verifica se o coordenador está funcionando
    
    // ========== Controle Individual dos Mecanismos ==========
    bool startMechanism(IPCMechanism mechanism); // Inicia um mecanismo específico
    bool stopMechanism(IPCMechanism mechanism);  // Para um mecanismo específico
    bool restartMechanism(IPCMechanism mechanism); // Reinicia um mecanismo (para + inicia)
    
    // ========== Interface de Comunicação ==========
    bool sendMessage(IPCMechanism mechanism, const std::string& message);  // Envia mensagem
    std::string receiveMessage(IPCMechanism mechanism);                     // Recebe mensagem
    
    // ========== Status e Monitoramento ==========
    CoordinatorStatus getFullStatus() const;     // Status completo do sistema inteiro
    MechanismStatus getMechanismStatus(IPCMechanism mechanism) const;  // Status de um mecanismo
    std::vector<std::string> getLogs(IPCMechanism mechanism, size_t count = 100);  // Logs recentes
    
    // ========== Interface para o Servidor HTTP ==========
    std::string executeCommand(const IPCCommand& command);  // Executa comando vindo da web e retorna JSON
    std::string getStatusJSON() const;           // Status completo em formato JSON
    std::string getMechanismDetailJSON(IPCMechanism mechanism) const; // Detalhes de um mecanismo em JSON
    void printStatus() const;                    // Imprime status no terminal
    
    // ========== Gerenciamento de Processos ==========
    void waitForAllChildren();                   // Espera todos os processos filhos terminarem
    void killAllChildren();                      // Força o término de todos os processos filhos
    
    // ========== Tratamento de Sinais do Sistema ==========
    static void signalHandler(int signal);      // Handler para SIGINT (CTRL+C) e SIGTERM
    void setupSignalHandlers();                 // Configura os handlers de sinal
    
    // ========== Funções Utilitárias ==========
    std::string getCurrentTimestamp() const;    // Retorna timestamp atual formatado

private:
    // ========== Managers dos Mecanismos IPC ==========
    // Cada manager é responsável por um tipo específico de comunicação
    std::unique_ptr<PipeManager> pipe_manager_;         // Gerencia pipes anônimos
    std::unique_ptr<SocketManager> socket_manager_;     // Gerencia Unix Domain Sockets
    std::unique_ptr<SharedMemoryManager> shmem_manager_; // Gerencia memória compartilhada
    
    // ========== Controle de Estado Thread-Safe ==========
    std::atomic<bool> is_running_;           // Se o coordenador está rodando (thread-safe)
    std::atomic<bool> shutdown_requested_;   // Se foi solicitado o shutdown (thread-safe)
    std::map<IPCMechanism, bool> mechanism_status_;  // Status de cada mecanismo (ativo/inativo)
    std::map<IPCMechanism, pid_t> mechanism_pids_;   // PIDs dos processos de cada mecanismo
    
    // ========== Threads de Monitoramento ==========
    std::vector<std::thread> monitoring_threads_;   // Threads que monitoram os mecanismos
    
    // ========== Dados de Status e Logging ==========
    std::string startup_time_;                       // Quando o sistema foi iniciado
    std::map<IPCMechanism, std::vector<std::string>> mechanism_logs_;  // Logs de cada mecanismo
    std::map<IPCMechanism, size_t> message_counts_;  // Contador de mensagens por mecanismo
    
    Logger& logger_;  // Referência ao sistema de logging
    
    // ========== Signal Handler Estático ==========
    static IPCCoordinator* instance_;  // Instância estática para o signal handler acessar
    
    // ========== Funções Auxiliares ==========
    std::string mechanismToString(IPCMechanism mech) const;    // Converte enum para string
    IPCMechanism stringToMechanism(const std::string& str) const; // Converte string para enum
    double getCurrentTimeMs() const;                           // Tempo atual em milissegundos
    
    // ========== Monitoramento de Processos ==========
    bool isProcessAlive(pid_t pid) const;  // Verifica se um processo ainda está executando
    
    // ========== Inicialização Individual dos Mecanismos ==========
    bool initializePipes();           // Inicializa apenas o sistema de pipes
    bool initializeSockets();         // Inicializa apenas o sistema de sockets
    bool initializeSharedMemory();    // Inicializa apenas a memória compartilhada
    
    // ========== Limpeza e Logging ==========
    void cleanup();  // Limpa todos os recursos ao desligar
    void logMechanismActivity(IPCMechanism mechanism, const std::string& activity);  // Registra atividades
};

} // namespace ipc_project
