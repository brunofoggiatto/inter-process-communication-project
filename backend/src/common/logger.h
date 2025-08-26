#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <format>

/**
 * @file logger.h
 * @brief Sistema de logging thread-safe pro projeto IPC
 */

namespace ipc_project {

// Niveis de log - do menos importante pro mais importante  
enum class LogLevel {
    DEBUG = 0,    // Info detalhada pra debug
    INFO = 1,     // informacao geral
    WARNING = 2,  // Algo estranho aconteceu
    ERROR = 3     // algo deu errado
};

// Classe principal de logging - escreve mensagens no arquivo e console
// Usa singleton pra todo mundo usar o mesmo logger
class Logger {
public:
    // nao permite copia - padrao singleton
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // Pega a unica instancia do logger
    static Logger& getInstance();
    
    // configura onde salvar as mensagens
    bool setLogFile(const std::string& filename);
    
    // Define nivel minimo - mensagens abaixo sao ignoradas
    void setLevel(LogLevel level);
    
    // funcao principal de log - todas outras chamam esta
    void log(LogLevel level, const std::string& message, const std::string& component = "");
    
    // Funcoes de conveniencia pra diferentes niveis
    void debug(const std::string& message, const std::string& component = "");
    void info(const std::string& message, const std::string& component = "");
    void warning(const std::string& message, const std::string& component = "");
    void error(const std::string& message, const std::string& component = "");
    
    // limpa - fecha o arquivo de log
    void close();

private:
    // Construtor privado pro singleton
    Logger() = default;
    ~Logger();
    
    // funcoes auxiliares
    std::string levelToString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
    std::string formatMessage(LogLevel level, const std::string& message, const std::string& component) const;

private:
    std::mutex mutex_;                        // Thread safety
    std::ofstream logFile_;                   // onde escrevemos os logs
    LogLevel currentLevel_ = LogLevel::INFO;  // Nivel minimo atual
    bool consoleOutput_ = true;               // tambem imprime na tela
};

// Macros pra facilitar o logging - so usar LOG_INFO("mensagem", "componente")
#define LOG_DEBUG(msg, comp) ipc_project::Logger::getInstance().debug(msg, comp)
#define LOG_INFO(msg, comp) ipc_project::Logger::getInstance().info(msg, comp)  
#define LOG_WARNING(msg, comp) ipc_project::Logger::getInstance().warning(msg, comp)
#define LOG_ERROR(msg, comp) ipc_project::Logger::getInstance().error(msg, comp)

// ainda mais simples - sem componente
#define LOG_D(msg) LOG_DEBUG(msg, "")
#define LOG_I(msg) LOG_INFO(msg, "")
#define LOG_W(msg) LOG_WARNING(msg, "")
#define LOG_E(msg) LOG_ERROR(msg, "")

} // namespace ipc_project