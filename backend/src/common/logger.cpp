/**
 * @file logger.cpp
 * @brief Implementação do sistema de logging thread-safe
 * 
 * Este arquivo implementa todas as funcionalidades da classe Logger:
 * - Singleton pattern (Meyer's Singleton)
 * - Thread safety com std::mutex
 * - Escrita simultânea em arquivo e console
 * - Formatação de mensagens com timestamps
 */

#include "logger.h"        // Header da classe Logger
#include <iostream>        // Para std::cout
#include <iomanip>         // Para formatação de datas/horas
#include <sstream>         // Para stringstream (construção de strings)

namespace ipc_project {

/**
 * Implementação do padrão Singleton usando Meyer's Singleton
 * Thread-safe desde C++11 - o compilador garante inicialização atômica
 * @return Referência para a única instância do Logger
 */
Logger& Logger::getInstance() {
    static Logger instance;  // Meyer's Singleton - mais simples e seguro que outros padrões
    return instance;         // Retorna sempre a mesma instância
}

/**
 * Destrutor - garante que o arquivo de log seja fechado adequadamente
 * Chamado automaticamente quando o programa termina
 */
Logger::~Logger() {
    close();  // Fecha o arquivo de log e limpa recursos
}

/**
 * Define o arquivo onde os logs serão salvos
 * @param filename Caminho para o arquivo de log
 * @return true se conseguiu abrir o arquivo, false caso contrário
 */
bool Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);  // Thread safety - apenas uma thread por vez
    
    // Fecha arquivo atual se já estiver aberto
    if (logFile_.is_open()) {
        logFile_.close();
    }
    
    // Tenta abrir novo arquivo em modo append (adiciona no final)
    logFile_.open(filename, std::ios::app);
    
    if (logFile_.is_open()) {
        // Escreve cabeçalho para marcar início de uma nova sessão de logging
        logFile_ << "\n" << std::string(50, '=') << "\n";
        logFile_ << "Logger inicializado: " << getCurrentTimestamp() << "\n";
        logFile_ << std::string(50, '=') << "\n";
        logFile_.flush();  // Força escrita imediata no disco
        return true;
    }
    
    return false;  // Falha ao abrir arquivo
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