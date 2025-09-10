/**
 * @file main.cpp
 * @brief Main entry point for the IPC system
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

// Global variable to control main loop
volatile bool keep_running = true;

// System signal handler
void signalHandler(int signal) {
    std::cout << "\nSignal received (" << signal << "), shutting down..." << std::endl;
    keep_running = false;
}

void printHelp() {
    std::cout << "\n=== IPC System - Inter-Process Communication ===\n"
              << "Usage: ./main [options]\n\n"
              << "Options:\n"
              << "  -h, --help     Show this help\n"
              << "  -d, --daemon   Run in daemon mode (no interaction)\n"
              << "  -s, --server   Run with integrated web server\n"
              << "  -i, --interactive  Interactive mode (default)\n"
              << "  -l, --log <file>  Set log file\n"
              << "  -v, --verbose  Verbose mode (DEBUG)\n"
              << "  -p, --port <n> HTTP port (default 9000)\n\n"
              << "Interactive commands:\n"
              << "  start <mechanism>  - Start mechanism (pipes|sockets|shmem)\n"
              << "  stop <mechanism>   - Stop mechanism\n"
              << "  send <mechanism> <message>  - Send message\n"
              << "  status             - Show status of all mechanisms\n"
              << "  logs <mechanism>   - Show mechanism logs\n"
              << "  help               - Show available commands\n"
              << "  quit, exit         - Exit program\n\n";
}

void printInteractiveHelp() {
    std::cout << "\n=== Available Commands ===\n"
              << "start pipes        - Start pipe communication\n"
              << "start sockets      - Start socket communication\n"
              << "start shmem        - Start shared memory\n"
              << "stop <mechanism>   - Stop specified mechanism\n"
              << "send pipes \"message\"    - Send message via pipes\n"
              << "send sockets \"message\"  - Send message via sockets\n"
              << "send shmem \"message\"    - Write to shared memory\n"
              << "status             - Show complete status\n"
              << "logs <mechanism>   - Show recent logs\n"
              << "help               - Show this help\n"
              << "quit / exit        - Exit\n\n";
}

IPCMechanism stringToMechanism(const std::string& str) {
    if (str == "pipes") return IPCMechanism::PIPES;
    if (str == "sockets") return IPCMechanism::SOCKETS;
    if (str == "shmem" || str == "shared_memory") return IPCMechanism::SHARED_MEMORY;
    return IPCMechanism::PIPES; // default
}

void interactiveMode(IPCCoordinator& coordinator) {
    std::string input;
    std::string command, param1, param2;
    
    std::cout << "\n=== Interactive IPC Mode ===\n"
              << "Type 'help' to see available commands\n"
              << "Type 'quit' to exit\n\n";
    
    while (keep_running && std::getline(std::cin, input)) {
        if (input.empty()) continue;
        
        // Simple command parsing
        std::istringstream iss(input);
        iss >> command;
        
        if (command == "quit" || command == "exit") {
            break;
        }
        else if (command == "help") {
            printInteractiveHelp();
        }
        else if (command == "status") {
            std::cout << "Current status:\n" << coordinator.getStatusJSON() << "\n\n";
        }
        else if (command == "start") {
            iss >> param1;
            if (param1.empty()) {
                std::cout << "Usage: start <pipes|sockets|shmem>\n";
                continue;
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.startMechanism(mech)) {
                std::cout << "✓ Mechanism " << param1 << " started successfully\n\n";
            } else {
                std::cout << "✗ Failed to start " << param1 << "\n\n";
            }
        }
        else if (command == "stop") {
            iss >> param1;
            if (param1.empty()) {
                std::cout << "Usage: stop <pipes|sockets|shmem>\n";
                continue;
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            if (coordinator.stopMechanism(mech)) {
                std::cout << "✓ Mechanism " << param1 << " stopped successfully\n\n";
            } else {
                std::cout << "✗ Failed to stop " << param1 << "\n\n";
            }
        }
        else if (command == "send") {
            iss >> param1;
            std::getline(iss, param2); // resto da linha como mensagem
            
            if (param1.empty() || param2.empty()) {
                std::cout << "Usage: send <mechanism> <message>\n";
                continue;
            }
            
            // Remove spaces at beginning of message
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
                std::cout << "✓ Message sent via " << param1 << ": \"" << param2 << "\"\n\n";
            } else {
                std::cout << "✗ Failed to send message via " << param1 << "\n\n";
            }
        }
        else if (command == "logs") {
            iss >> param1;
            if (param1.empty()) {
                std::cout << "Usage: logs <pipes|sockets|shmem>\n";
                continue;
            }
            
            IPCMechanism mech = stringToMechanism(param1);
            auto logs = coordinator.getLogs(mech, 20); // last 20 logs
            
            std::cout << "Logs for " << param1 << ":\n";
            if (logs.empty()) {
                std::cout << "(no logs available)\n";
            } else {
                for (const auto& log : logs) {
                    std::cout << log << "\n";
                }
            }
            std::cout << "\n";
        }
        else {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Type 'help' to see available commands\n\n";
        }
    }
}

void serverMode(IPCCoordinator& coordinator, int http_port) {
    std::cout << "Starting integrated web server mode...\n";
    
    // Start all mechanisms
    coordinator.startMechanism(IPCMechanism::PIPES);
    coordinator.startMechanism(IPCMechanism::SOCKETS);  
    coordinator.startMechanism(IPCMechanism::SHARED_MEMORY);
    
    std::cout << "✓ IPC mechanisms started\n";
    
    // Create and start HTTP server
    HTTPServer server(http_port);
    server.setIPCCoordinator(std::shared_ptr<IPCCoordinator>(&coordinator, [](IPCCoordinator*) {}));
    
    // Configure path for static files (frontend)
    // Try paths relative to executable/build location for portability
    {
        std::string staticPath = "./frontend";
        try {
            namespace fs = std::filesystem;
            const std::vector<std::string> candidates = {
                "../../frontend", // typical: build/bin -> repo/frontend
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
            // fallback to ./frontend
        }
        server.setStaticPath(staticPath);
    }
    
    // Start server
    if (!server.start()) {
        // Port busy? try next 10 ports
        bool started = false;
        for (int p = http_port + 1; p <= http_port + 10; ++p) {
            server.setPort(p);
            if (server.start()) { started = true; http_port = p; break; }
        }
        if (!started) {
            std::cerr << "❌ Error starting HTTP server! Port busy and fallback attempts failed.\n";
            std::cerr << "Suggestion: use --port <n> or free the port with 'lsof -i :" << http_port << "'" << std::endl;
            return;
        }
    }
    
    std::cout << "✓ HTTP server started on port " << http_port << "\n";
    std::cout << "✓ Access: http://localhost:" << http_port << "/\n";
    std::cout << "Initial status:\n" << coordinator.getStatusJSON() << "\n\n";
    
    // Main server loop
    while (keep_running && coordinator.isRunning()) {
        coordinator.waitForAllChildren();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Status every 30 seconds
        static int counter = 0;
        if (++counter >= 300) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "Server status [" << std::ctime(&time_t) << "]:\n";
            std::cout << coordinator.getStatusJSON() << "\n\n";
            counter = 0;
        }
    }
    
    std::cout << "Stopping HTTP server...\n";
    server.stop();
    std::cout << "Server stopped.\n";
}

void daemonMode(IPCCoordinator& coordinator) {
    std::cout << "Starting daemon mode...\n";
    
    // Start all mechanisms
    coordinator.startMechanism(IPCMechanism::PIPES);
    coordinator.startMechanism(IPCMechanism::SOCKETS);  
    coordinator.startMechanism(IPCMechanism::SHARED_MEMORY);
    
    std::cout << "Initial status:\n" << coordinator.getStatusJSON() << "\n\n";
    
    // Main daemon loop
    while (keep_running && coordinator.isRunning()) {
        // Wait for child processes to finish if necessary
        coordinator.waitForAllChildren();
        
        // Small pause to not consume 100% CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Every 30 seconds, print status
        static int counter = 0;
        if (++counter >= 300) { // 300 * 100ms = 30s
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "Daemon status [" << std::ctime(&time_t) << "]:\n";
            std::cout << coordinator.getStatusJSON() << "\n\n";
            counter = 0;
        }
    }
    
    std::cout << "Daemon shutting down...\n";
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
