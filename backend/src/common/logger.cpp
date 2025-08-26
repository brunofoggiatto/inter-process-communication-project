/**
 * @file logger.cpp
 * @brief Implementacao do sistema de logging
 */

#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace ipc_project {

// variavel estatica guarda a unica instancia - thread-safe desde C++11
Logger& Logger::getInstance() {
    static Logger instance;  // meyer's singleton - mais simples que outros padroes
    return instance;
}

// destrutor - garante que o arquivo seja fechado
Logger::~Logger() {
    close();
}

bool Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);  // Thread safety
    
    // Fecha arquivo atual se estiver aberto
    if (logFile_.is_open()) {
        logFile_.close();
    }
    
    // Tenta abrir novo arquivo em modo append
    logFile_.open(filename, std::ios::app);
    
    if (logFile_.is_open()) {
        // Escreve cabecalho pra mostrar quando o logging comecou
        logFile_ << "\n" << std::string(50, '=') << "\n";
        logFile_ << "Logger initialized: " << getCurrentTimestamp() << "\n";
        logFile_ << std::string(50, '=') << "\n";
        logFile_.flush();  // garante que seja escrito agora
        return true;
    }
    
    return false;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
    
    // Loga que mudamos o nivel
    std::string levelStr = levelToString(level);
    std::string message = "Log level changed to: " + levelStr;
    
    // Escreve diretamente pra evitar recursao
    if (consoleOutput_) {
        std::cout << "[INFO] " << getCurrentTimestamp() << " [LOGGER] " << message << std::endl;
    }
    if (logFile_.is_open()) {
        logFile_ << "[INFO] " << getCurrentTimestamp() << " [LOGGER] " << message << std::endl;
        logFile_.flush();
    }
}

void Logger::log(LogLevel level, const std::string& message, const std::string& component) {
    // pula se o nivel eh muito baixo
    if (level < currentLevel_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);  // thread safety
    
    // formata a mensagem com timestamp e nivel  
    std::string msg_formatada = formatMessage(level, message, component);
    
    // escreve no console se habilitado
    if (consoleOutput_) {
        // erros e warnings vao pra stderr, info e debug pro stdout
        if (level >= LogLevel::WARNING) {
            std::cerr << msg_formatada << std::endl;
        } else {
            std::cout << msg_formatada << std::endl;
        }
    }
    
    // escreve no arquivo se temos um aberto
    if (logFile_.is_open()) {
        logFile_ << msg_formatada << std::endl;
        logFile_.flush();  // garante que é escrito imediatamente
    }
}

// estas funcoes so chamam a funcao principal de log com o nivel certo
void Logger::debug(const std::string& message, const std::string& component) {
    log(LogLevel::DEBUG, message, component);
}

void Logger::info(const std::string& message, const std::string& component) {
    log(LogLevel::INFO, message, component);
}

void Logger::warning(const std::string& message, const std::string& component) {
    log(LogLevel::WARNING, message, component);
}

void Logger::error(const std::string& message, const std::string& component) {
    log(LogLevel::ERROR, message, component);
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFile_.is_open()) {
        // Escreve rodape pra mostrar quando o logging terminou
        logFile_ << std::string(50, '=') << "\n";
        logFile_ << "Logger finalized: " << getCurrentTimestamp() << "\n";
        logFile_ << std::string(50, '=') << "\n\n";
        logFile_.close();
    }
}

// Converte o enum pra string legivel
std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

// pega tempo atual como string formatada
std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%d/%m/%Y %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// Junta o formato final da mensagem de log
std::string Logger::formatMessage(LogLevel level, const std::string& message, 
                                const std::string& component) const {
    std::stringstream ss;
    
    // Formato: [NIVEL] timestamp [COMPONENTE] mensagem
    ss << "[" << levelToString(level) << "] ";
    ss << getCurrentTimestamp() << " ";
    
    // Adiciona nome do componente se fornecido
    if (!component.empty()) {
        ss << "[" << component << "] ";
    }
    
    ss << message;
    
    return ss.str();
}

} // namespace ipc_project