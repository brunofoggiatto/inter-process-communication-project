/**
 * @file main.cpp
 * @brief Ponto de entrada principal do sistema IPC
 * Este arquivo contém a lógica principal para inicializar e gerenciar 
 * todos os mecanismos de comunicação inter-processos
 */

// Includes necessários para o funcionamento do sistema
#include <iostream>      // Para entrada e saída padrão (cout, cin)
#include <string>        // Para manipulação de strings
#include <thread>        // Para threading (usado no servidor HTTP)
#include <chrono>        // Para medição de tempo e sleep
#include <csignal>       // Para tratamento de sinais do sistema (CTRL+C, etc)
#include <sstream>       // Para parsing de comandos do usuário
#include <filesystem>    // Para operações com arquivos e diretórios
#include <cstdlib>       // Para funções C padrão (atoi, etc)

// Headers específicos do projeto
#include "ipc/ipc_coordinator.h"  // Coordenador principal de todos os mecanismos IPC
#include "common/logger.h"        // Sistema de logging
#include "server/http_server.h"   // Servidor HTTP para interface web

using namespace ipc_project;  // Usa o namespace do projeto para evitar prefixos

// Variável global para controlar o loop principal do programa
// volatile garante que não seja otimizada pelo compilador
volatile bool keep_running = true;

/**
 * Handler para sinais do sistema (SIGINT=CTRL+C, SIGTERM=kill)
 * Esta função é chamada automaticamente quando o programa recebe um sinal
 */
void signalHandler(int signal) {
    std::cout << "\nSinal recebido (" << signal << "), desligando sistema..." << std::endl;
    keep_running = false;  // Sinaliza para todos os loops pararem
}

/**
 * Função que mostra a ajuda do programa com todas as opções disponíveis
 */
void printHelp() {
    std::cout << "\n=== Sistema IPC - Comunicação Inter-Processos ===\n"
              << "Uso: ./main [opções]\n\n"
              << "Opções da linha de comando:\n"
              << "  -h, --help     Mostra esta ajuda\n"
              << "  -d, --daemon   Roda em modo daemon (sem interação)\n"
              << "  -s, --server   Roda com servidor web integrado\n"
              << "  -i, --interactive  Modo interativo (padrão)\n"
              << "  -l, --log <arquivo>  Define arquivo de log\n"
              << "  -v, --verbose  Modo verboso (DEBUG)\n"
              << "  -p, --port <n> Porta HTTP (padrão 9000)\n\n"
              << "Comandos do modo interativo:\n"
              << "  start <mecanismo>  - Inicia mecanismo (pipes|sockets|shmem)\n"
              << "  stop <mecanismo>   - Para mecanismo\n"
              << "  send <mecanismo> <mensagem>  - Envia mensagem\n"
              << "  status             - Mostra status de todos os mecanismos\n"
              << "  logs <mecanismo>   - Mostra logs do mecanismo\n"
              << "  help               - Mostra comandos disponíveis\n"
              << "  quit, exit         - Sai do programa\n\n";
}

/**
 * Função que mostra ajuda detalhada dos comandos interativos
 */
void printInteractiveHelp() {
    std::cout << "\n=== Comandos Disponíveis ===\n"
              << "start pipes        - Inicia comunicação via pipes\n"
              << "start sockets      - Inicia comunicação via sockets\n"
              << "start shmem        - Inicia memória compartilhada\n"
              << "stop <mecanismo>   - Para o mecanismo especificado\n"
              << "send pipes \"mensagem\"    - Envia mensagem via pipes\n"
              << "send sockets \"mensagem\"  - Envia mensagem via sockets\n"
              << "send shmem \"mensagem\"    - Escreve na memória compartilhada\n"
              << "status             - Mostra status completo\n"
              << "logs <mecanismo>   - Mostra logs recentes\n"
              << "help               - Mostra esta ajuda\n"
              << "quit / exit        - Sair\n\n";
}

/**
 * Converte string em enum IPCMechanism
 * @param str Nome do mecanismo como string
 * @return Enum correspondente ao mecanismo
 */
IPCMechanism stringToMechanism(const std::string& str) {
    if (str == "pipes") return IPCMechanism::PIPES;
    if (str == "sockets") return IPCMechanism::SOCKETS;
    if (str == "shmem" || str == "shared_memory") return IPCMechanism::SHARED_MEMORY;
    return IPCMechanism::PIPES; // padrão se não reconhecer
}

/**
 * Função principal do modo interativo
 * Permite ao usuário digitar comandos para controlar os mecanismos IPC
 * @param coordinator Referência ao coordenador IPC que gerencia todos os mecanismos
 */
void interactiveMode(IPCCoordinator& coordinator) {
    std::string input;        // String para armazenar entrada do usuário
    std::string command, param1, param2;  // Variáveis para parsing dos comandos
    
    std::cout << "\n=== Modo IPC Interativo ===\n"
              << "Digite 'help' para ver comandos disponíveis\n"
              << "Digite 'quit' para sair\n\n";
    
    // Loop principal - continua até usuário sair ou sistema ser interrompido
    while (keep_running && std::getline(std::cin, input)) {
        if (input.empty()) continue;  // Ignora linhas vazias
        
        // Parse simples do comando - separa por espaços
        std::istringstream iss(input);
        iss >> command;  // Primeiro token é o comando
        
        // Processa cada comando possível
        if (command == "quit" || command == "exit") {
            break;  // Sai do loop e termina programa
        }
        else if (command == "help") {
            printInteractiveHelp();  // Mostra ajuda dos comandos
        }
        else if (command == "status") {
            // Mostra status atual de todos os mecanismos em formato JSON
            std::cout << "Status atual:\n" << coordinator.getStatusJSON() << "\n\n";
        }
        else if (command == "start") {
            // Comando para iniciar um mecanismo específico
            iss >> param1;  // Lê o nome do mecanismo
            if (param1.empty()) {
                std::cout << "Uso: start <pipes|sockets|shmem>\n";
                continue;
            }
            
            // Converte string para enum e tenta iniciar o mecanismo
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.startMechanism(mech)) {
                std::cout << "✓ Mecanismo " << param1 << " iniciado com sucesso\n\n";
            } else {
                std::cout << "✗ Falha ao iniciar " << param1 << "\n\n";
            }
        }
        else if (command == "stop") {
            // Comando para parar um mecanismo específico
            iss >> param1;  // Lê o nome do mecanismo
            if (param1.empty()) {
                std::cout << "Uso: stop <pipes|sockets|shmem>\n";
                continue;
            }
            
            // Converte string para enum e tenta parar o mecanismo
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.stopMechanism(mech)) {
                std::cout << "✓ Mecanismo " << param1 << " parado com sucesso\n\n";
            } else {
                std::cout << "✗ Falha ao parar " << param1 << "\n\n";
            }
        }
        else if (command == "send") {
            // Comando para enviar mensagem via um mecanismo específico
            iss >> param1;  // Nome do mecanismo
            std::getline(iss, param2); // Resto da linha como mensagem
            
            if (param1.empty() || param2.empty()) {
                std::cout << "Uso: send <mecanismo> <mensagem>\n";
                continue;
            }
            
            // Remove espaços no início da mensagem
            size_t start = param2.find_first_not_of(" \t");
            if (start != std::string::npos) {
                param2 = param2.substr(start);
            }
            
            // Remove aspas se existirem (permite mensagens com espaços)
            if (param2.front() == '"' && param2.back() == '"') {
                param2 = param2.substr(1, param2.length() - 2);
            }
            
            // Converte mecanismo e envia mensagem
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.sendMessage(mech, param2)) {
                std::cout << "✓ Mensagem enviada via " << param1 << ": \"" << param2 << "\"\n\n";
            } else {
                std::cout << "✗ Falha ao enviar mensagem via " << param1 << "\n\n";
            }
        }
        else if (command == "logs") {
            // Comando para mostrar logs de um mecanismo específico
            iss >> param1;  // Nome do mecanismo
            if (param1.empty()) {
                std::cout << "Uso: logs <pipes|sockets|shmem>\n";
                continue;
            }
            
            // Obtém os últimos 20 logs do mecanismo
            IPCMechanism mech = stringToMechanism(param1);
            auto logs = coordinator.getLogs(mech, 20);
            
            std::cout << "Logs para " << param1 << ":\n";
            if (logs.empty()) {
                std::cout << "(nenhum log disponível)\n";
            } else {
                // Exibe cada linha de log
                for (const auto& log : logs) {
                    std::cout << log << "\n";
                }
            }
            std::cout << "\n";
        }
        else {
            // Comando não reconhecido
            std::cout << "Comando desconhecido: " << command << "\n";
            std::cout << "Digite 'help' para ver comandos disponíveis\n\n";
        }
    }
}

/**
 * Função do modo servidor - inicia servidor HTTP integrado
 * @param coordinator Referência ao coordenador IPC
 * @param http_port Porta para o servidor HTTP
 */
void serverMode(IPCCoordinator& coordinator, int http_port) {
    std::cout << "Iniciando modo servidor web integrado...\n";
    
    // Inicia todos os mecanismos automaticamente no modo servidor
    coordinator.startMechanism(IPCMechanism::PIPES);
    coordinator.startMechanism(IPCMechanism::SOCKETS);  
    coordinator.startMechanism(IPCMechanism::SHARED_MEMORY);
    
    std::cout << "✓ Mecanismos IPC iniciados\n";
    
    // Cria e configura o servidor HTTP
    HTTPServer server(http_port);
    // Configura o coordenador IPC no servidor (usando shared_ptr com deletor vazio)
    server.setIPCCoordinator(std::shared_ptr<IPCCoordinator>(&coordinator, [](IPCCoordinator*) {}));
    
    // Configura caminho para arquivos estáticos (frontend web)
    // Tenta múltiplos caminhos relativos para máxima portabilidade
    {
        std::string staticPath = "./frontend";
        try {
            namespace fs = std::filesystem;
            // Lista de caminhos candidatos (do mais provável para o menos)
            const std::vector<std::string> candidates = {
                "../../frontend", // típico: build/bin -> repo/frontend
                "../frontend",    // alternativo: build -> repo/frontend  
                "./frontend"      // local: mesmo diretório
            };
            // Testa cada caminho procurando por index.html
            for (const auto& c : candidates) {
                fs::path p = fs::path(c) / "index.html";
                if (fs::exists(p)) {
                    staticPath = c;  // Encontrou! Usa este caminho
                    break;
                }
            }
        } catch (...) {
            // Se falhar, usa ./frontend como fallback
        }
        server.setStaticPath(staticPath);  // Configura no servidor
    }
    
    // Tenta iniciar o servidor na porta especificada
    if (!server.start()) {
        // Porta ocupada? Tenta as próximas 10 portas automaticamente
        bool started = false;
        for (int p = http_port + 1; p <= http_port + 10; ++p) {
            server.setPort(p);
            if (server.start()) { 
                started = true; 
                http_port = p; 
                break; 
            }
        }
        if (!started) {
            std::cerr << "❌ Erro ao iniciar servidor HTTP! Porta ocupada e tentativas de fallback falharam.\n";
            std::cerr << "Sugestão: use --port <n> ou libere a porta com 'lsof -i :" << http_port << "'" << std::endl;
            return;
        }
    }
    
    std::cout << "✓ Servidor HTTP iniciado na porta " << http_port << "\n";
    std::cout << "✓ Acesso: http://localhost:" << http_port << "/\n";
    std::cout << "Status inicial:\n" << coordinator.getStatusJSON() << "\n\n";
    
    // Loop principal do servidor (roda até CTRL+C ou erro)
    while (keep_running && coordinator.isRunning()) {
        coordinator.waitForAllChildren();  // Espera processos filhos IPC
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Evita 100% CPU
        
        // Mostra status a cada 30 segundos (300 * 100ms = 30s)
        static int counter = 0;
        if (++counter >= 300) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "Status do servidor [" << std::ctime(&time_t) << "]:\n";
            std::cout << coordinator.getStatusJSON() << "\n\n";
            counter = 0;  // Reset contador
        }
    }
    
    std::cout << "Parando servidor HTTP...\n";
    server.stop();  // Para servidor graciosamente
    std::cout << "Servidor parado.\n";
}

/**
 * Função do modo daemon - roda em background sem interação
 * @param coordinator Referência ao coordenador IPC
 */
void daemonMode(IPCCoordinator& coordinator) {
    std::cout << "Iniciando modo daemon...\n";
    
    // Inicia todos os mecanismos automaticamente
    coordinator.startMechanism(IPCMechanism::PIPES);
    coordinator.startMechanism(IPCMechanism::SOCKETS);  
    coordinator.startMechanism(IPCMechanism::SHARED_MEMORY);
    
    std::cout << "Status inicial:\n" << coordinator.getStatusJSON() << "\n\n";
    
    // Loop principal do daemon (roda até ser interrompido)
    while (keep_running && coordinator.isRunning()) {
        // Espera processos filhos terminarem se necessário
        coordinator.waitForAllChildren();
        
        // Pequena pausa para não consumir 100% da CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // A cada 30 segundos, imprime status
        static int counter = 0;
        if (++counter >= 300) { // 300 * 100ms = 30s
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "Status do daemon [" << std::ctime(&time_t) << "]:\n";
            std::cout << coordinator.getStatusJSON() << "\n\n";
            counter = 0;  // Reset contador
        }
    }
    
    std::cout << "Daemon desligando...\n";
}

int main(int argc, char* argv[]) {
    // Initial configuration
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    bool interactive_mode = true;
    bool server_mode = false;
    bool verbose = false;
    std::string log_file = "";
    int http_port = 9000;
    
    // Command line argument parsing
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
                    std::cerr << "Error: invalid port: " << http_port << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: option -p requires port number\n";
                return 1;
            }
        }
        else if (arg == "-l" || arg == "--log") {
            if (i + 1 < argc) {
                log_file = argv[++i];
            } else {
                std::cerr << "Error: option -l requires filename\n";
                return 1;
            }
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use -h to see available options\n";
            return 1;
        }
    }
    
    // Logger configuration
    Logger& logger = Logger::getInstance();
    
    if (verbose) {
        logger.setLevel(LogLevel::DEBUG);
    } else {
        logger.setLevel(LogLevel::INFO);
    }
    
    if (!log_file.empty()) {
        if (!logger.setLogFile(log_file)) {
            std::cerr << "Error configuring log file: " << log_file << "\n";
            return 1;
        }
    }
    
    std::cout << "=== Inter-Process Communication System ===\n";
    std::cout << "Mode: " << (interactive_mode ? "Interactive" : "Daemon") << "\n";
    std::cout << "Log level: " << (verbose ? "DEBUG" : "INFO") << "\n";
    std::cout << "HTTP port: " << http_port << "\n";
    if (!log_file.empty()) {
        std::cout << "Log file: " << log_file << "\n";
    }
    std::cout << "\n";
    
    try {
        // Coordinator initialization
        IPCCoordinator coordinator;
        
        if (!coordinator.initialize()) {
            std::cerr << "Error: Failed to initialize IPC coordinator\n";
            return 1;
        }
        
        std::cout << "✓ IPC coordinator initialized successfully\n\n";
        
        // Mode-based execution
        if (interactive_mode) {
            interactiveMode(coordinator);
        } else if (server_mode) {
            serverMode(coordinator, http_port);
        } else {
            daemonMode(coordinator);
        }
        
        std::cout << "Shutting down system...\n";
        coordinator.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "IPC system terminated.\n";
    return 0;
}
