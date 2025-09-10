/**
 * @file main.cpp
 * @brief Ponto de entrada principal para o sistema IPC
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include "ipc/ipc_coordinator.h"
#include "common/logger.h"
#include "server/http_server.h"

using namespace ipc_project;

// Variável global pra controlar o loop principal
volatile bool keep_running = true;

// Handler pra sinais do sistema
void signalHandler(int signal) {
    std::cout << "\nSinal recebido (" << signal << "), encerrando..." << std::endl;
    keep_running = false;
}

void printHelp() {
    std::cout << "\n=== Sistema IPC - Comunicação Entre Processos ===\n"
              << "Uso: ./main [opções]\n\n"
              << "Opções:\n"
              << "  -h, --help     Mostra esta ajuda\n"
              << "  -d, --daemon   Executa em modo daemon (sem interação)\n"
              << "  -s, --server   Executa com servidor web integrado\n"
              << "  -i, --interactive  Modo interativo (padrão)\n"
              << "  -l, --log <arquivo>  Define arquivo de log\n"
              << "  -v, --verbose  Modo verbose (DEBUG)\n"
              << "  -p, --port <n> Porta HTTP (padrão 9000)\n\n"
              << "Comandos interativos:\n"
              << "  start <mecanismo>  - Liga mecanismo (pipes|sockets|shmem)\n"
              << "  stop <mecanismo>   - Para mecanismo\n"
              << "  send <mecanismo> <mensagem>  - Envia mensagem\n"
              << "  status             - Mostra status de todos os mecanismos\n"
              << "  logs <mecanismo>   - Mostra logs do mecanismo\n"
              << "  help               - Mostra comandos disponíveis\n"
              << "  quit, exit         - Sair do programa\n\n";
}

void printInteractiveHelp() {
    std::cout << "\n=== Comandos Disponíveis ===\n"
              << "start pipes        - Inicia comunicação por pipes\n"
              << "start sockets      - Inicia comunicação por sockets\n"
              << "start shmem        - Inicia memória compartilhada\n"
              << "stop <mecanismo>   - Para o mecanismo especificado\n"
              << "send pipes \"mensagem\"    - Envia mensagem via pipes\n"
              << "send sockets \"mensagem\"  - Envia mensagem via sockets\n"
              << "send shmem \"mensagem\"    - Escreve na memória compartilhada\n"
              << "status             - Mostra status completo\n"
              << "logs <mecanismo>   - Mostra últimos logs\n"
              << "help               - Mostra esta ajuda\n"
              << "quit / exit        - Sair\n\n";
}

IPCMechanism stringToMechanism(const std::string& str) {
    if (str == "pipes") return IPCMechanism::PIPES;
    if (str == "sockets") return IPCMechanism::SOCKETS;
    if (str == "shmem" || str == "shared_memory") return IPCMechanism::SHARED_MEMORY;
    return IPCMechanism::PIPES; // padrão
}

void interactiveMode(IPCCoordinator& coordinator) {
    std::string input;
    std::string command, param1, param2;
    
    std::cout << "\n=== Modo Interativo IPC ===\n"
              << "Digite 'help' para ver comandos disponíveis\n"
              << "Digite 'quit' para sair\n\n";
    
    while (keep_running && std::getline(std::cin, input)) {
        if (input.empty()) continue;
        
        // Parse simples do comando
        std::istringstream iss(input);
        iss >> command;
        
        if (command == "quit" || command == "exit") {
            break;
        }
        else if (command == "help") {
            printInteractiveHelp();
        }
        else if (command == "status") {
            std::cout << "Status atual:\n" << coordinator.getStatusJSON() << "\n\n";
        }
        else if (command == "start") {
            iss >> param1;
            if (param1.empty()) {
                std::cout << "Uso: start <pipes|sockets|shmem>\n";
                continue;
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.startMechanism(mech)) {
                std::cout << "✓ Mecanismo " << param1 << " iniciado com sucesso\n\n";
            } else {
                std::cout << "✗ Falha ao iniciar " << param1 << "\n\n";
            }
        }
        else if (command == "stop") {
            iss >> param1;
            if (param1.empty()) {
                std::cout << "Uso: stop <pipes|sockets|shmem>\n";
                continue;
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.stopMechanism(mech)) {
                std::cout << "✓ Mecanismo " << param1 << " parado com sucesso\n\n";
            } else {
                std::cout << "✗ Falha ao parar " << param1 << "\n\n";
            }
        }
        else if (command == "send") {
            iss >> param1;
            std::getline(iss, param2); // resto da linha como mensagem
            
            if (param1.empty() || param2.empty()) {
                std::cout << "Uso: send <mecanismo> <mensagem>\n";
                continue;
            }
            
            // Remove espaços no início da mensagem
            size_t start = param2.find_first_not_of(" \t");
            if (start != std::string::npos) {
                param2 = param2.substr(start);
            }
            
            // Remove aspas se existirem
            if (param2.front() == '"' && param2.back() == '"') {
                param2 = param2.substr(1, param2.length() - 2);
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.sendMessage(mech, param2)) {
                std::cout << "✓ Mensagem enviada via " << param1 << ": \"" << param2 << "\"\n\n";
            } else {
                std::cout << "✗ Falha ao enviar mensagem via " << param1 << "\n\n";
            }
        }
        else if (command == "logs") {
            iss >> param1;
            if (param1.empty()) {
                std::cout << "Uso: logs <pipes|sockets|shmem>\n";
                continue;
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            auto logs = coordinator.getLogs(mech, 20); // últimos 20 logs
            
            std::cout << "Logs de " << param1 << ":\n";
            if (logs.empty()) {
                std::cout << "(nenhum log disponível)\n";
            } else {
                for (const auto& log : logs) {
                    std::cout << log << "\n";
                }
            }
            std::cout << "\n";
        }
        else {
            std::cout << "Comando desconhecido: " << command << "\n";
            std::cout << "Digite 'help' para ver comandos disponíveis\n\n";
        }
    }
}

void serverMode(IPCCoordinator& coordinator, int http_port) {
    std::cout << "Iniciando modo servidor web integrado...\n";
    
    // Inicia todos os mecanismos
    coordinator.startMechanism(IPCMechanism::PIPES);
    coordinator.startMechanism(IPCMechanism::SOCKETS);  
    coordinator.startMechanism(IPCMechanism::SHARED_MEMORY);
    
    std::cout << "✓ Mecanismos IPC iniciados\n";
    
    // Cria e inicia o servidor HTTP
    HTTPServer server(http_port);
    server.setIPCCoordinator(std::shared_ptr<IPCCoordinator>(&coordinator, [](IPCCoordinator*) {}));
    
    // Configura path para arquivos estáticos (frontend)
    // Tenta caminhos relativos ao local do executável/build para portabilidade
    {
        std::string staticPath = "./frontend";
        try {
            namespace fs = std::filesystem;
            const std::vector<std::string> candidates = {
                "../../frontend", // típico: build/bin -> repo/frontend
                "../frontend",
                "./frontend"
            };
            for (const auto& c : candidates) {
                fs::path p = fs::path(c) / "index.html";
                if (fs::exists(p)) {
                    staticPath = c;
                    break;
                }
            }
        } catch (...) {
            // fallback para ./frontend
        }
        server.setStaticPath(staticPath);
    }
    
    // Inicia o servidor
    if (!server.start()) {
        // Porta ocupada? tenta próximas 10 portas
        bool started = false;
        for (int p = http_port + 1; p <= http_port + 10; ++p) {
            server.setPort(p);
            if (server.start()) { started = true; http_port = p; break; }
        }
        if (!started) {
            std::cerr << "❌ Erro ao iniciar servidor HTTP! Porta ocupada e tentativas de fallback falharam.\n";
            std::cerr << "Sugestão: usar --port <n> ou liberar a porta com 'lsof -i :" << http_port << "'" << std::endl;
            return;
        }
    }
    
    std::cout << "✓ Servidor HTTP iniciado na porta " << http_port << "\n";
    std::cout << "✓ Acesse: http://localhost:" << http_port << "/\n";
    std::cout << "Status inicial:\n" << coordinator.getStatusJSON() << "\n\n";
    
    // Loop principal do servidor
    while (keep_running && coordinator.isRunning()) {
        coordinator.waitForAllChildren();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Status a cada 30 segundos
        static int counter = 0;
        if (++counter >= 300) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "Status servidor [" << std::ctime(&time_t) << "]:\n";
            std::cout << coordinator.getStatusJSON() << "\n\n";
            counter = 0;
        }
    }
    
    std::cout << "Parando servidor HTTP...\n";
    server.stop();
    std::cout << "Servidor encerrado.\n";
}

void daemonMode(IPCCoordinator& coordinator) {
    std::cout << "Iniciando modo daemon...\n";
    
    // Inicia todos os mecanismos
    coordinator.startMechanism(IPCMechanism::PIPES);
    coordinator.startMechanism(IPCMechanism::SOCKETS);  
    coordinator.startMechanism(IPCMechanism::SHARED_MEMORY);
    
    std::cout << "Status inicial:\n" << coordinator.getStatusJSON() << "\n\n";
    
    // Loop principal do daemon
    while (keep_running && coordinator.isRunning()) {
        // Aguarda processos filhos terminarem se necessário
        coordinator.waitForAllChildren();
        
        // Pequena pausa pra não consumir 100% da CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // A cada 30 segundos, imprime status
        static int counter = 0;
        if (++counter >= 300) { // 300 * 100ms = 30s
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "Status daemon [" << std::ctime(&time_t) << "]:\n";
            std::cout << coordinator.getStatusJSON() << "\n\n";
            counter = 0;
        }
    }
    
    std::cout << "Daemon encerrando...\n";
}

int main(int argc, char* argv[]) {
    // Configuração inicial
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    bool interactive_mode = true;
    bool server_mode = false;
    bool verbose = false;
    std::string log_file = "";
    int http_port = 9000;
    
    // Parse de argumentos da linha de comando
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
        else if (arg == "-d" || arg == "--daemon") {
            interactive_mode = false;
            server_mode = false;
        }
        else if (arg == "-s" || arg == "--server") {
            interactive_mode = false;
            server_mode = true;
        }
        else if (arg == "-i" || arg == "--interactive") {
            interactive_mode = true;
            server_mode = false;
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                http_port = std::atoi(argv[++i]);
                if (http_port <= 0 || http_port > 65535) {
                    std::cerr << "Erro: porta inválida: " << http_port << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Erro: opção -p requer número da porta\n";
                return 1;
            }
        }
        else if (arg == "-l" || arg == "--log") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            } else {
                std::cerr << "Erro: opção -l requer nome do arquivo\n";
                return 1;
            }
        }
        else {
            std::cerr << "Opção desconhecida: " << arg << "\n";
            std::cerr << "Use -h para ver opções disponíveis\n";
            return 1;
        }
    }
    
    // Configuração do logger
    Logger& logger = Logger::getInstance();
    
    if (verbose) {
        logger.setLevel(LogLevel::DEBUG);
    } else {
        logger.setLevel(LogLevel::INFO);
    }
    
    if (!log_file.empty()) {
        if (!logger.setLogFile(log_file)) {
            std::cerr << "Erro ao configurar arquivo de log: " << log_file << "\n";
            return 1;
        }
    }
    
    std::cout << "=== Sistema de Comunicação Inter-Processo ===\n";
    std::cout << "Modo: " << (interactive_mode ? "Interativo" : "Daemon") << "\n";
    std::cout << "Log level: " << (verbose ? "DEBUG" : "INFO") << "\n";
    std::cout << "HTTP port: " << http_port << "\n";
    if (!log_file.empty()) {
        std::cout << "Log file: " << log_file << "\n";
    }
    std::cout << "\n";
    
    try {
        // Inicialização do coordenador
        IPCCoordinator coordinator;
        
        if (!coordinator.initialize()) {
            std::cerr << "Erro: Falha ao inicializar coordenador IPC\n";
            return 1;
        }
        
        std::cout << "✓ Coordenador IPC inicializado com sucesso\n\n";
        
        // Execução baseada no modo
        if (interactive_mode) {
            interactiveMode(coordinator);
        } else if (server_mode) {
            serverMode(coordinator, http_port);
        } else {
            daemonMode(coordinator);
        }
        
        std::cout << "Encerrando sistema...\n";
        coordinator.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Erro fatal: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Sistema IPC encerrado.\n";
    return 0;
}
