#pragma once  // Garante inclusão única

#include <string>     // Para manipulação de strings
#include <fstream>    // Para escrita em arquivo
#include <iostream>   // Para saída no console
#include <mutex>      // Para thread safety
#include <chrono>     // Para timestamps
#include <format>     // Para formatação de strings (C++20)

/**
 * @file logger.h
 * @brief Sistema de logging thread-safe para o projeto IPC
 * 
 * Este arquivo implementa um sistema completo de logging que:
 * - Escreve simultaneamente em arquivo e console
 * - É thread-safe (várias threads podem usar simultaneamente)
 * - Usa padrão Singleton (uma instância global)
 * - Suporte a diferentes níveis de log (DEBUG, INFO, WARNING, ERROR)
 */

namespace ipc_project {

/**
 * Níveis de log ordenados por importância (do menos para o mais crítico)
 * Usado para filtrar mensagens - ex: se nível = WARNING, só mostra WARNING e ERROR
 */
enum class LogLevel {
    DEBUG = 0,    // Informações detalhadas para debug/desenvolvimento
    INFO = 1,     // Informações gerais do funcionamento normal
    WARNING = 2,  // Algo estranho aconteceu, mas o sistema continua funcionando
    ERROR = 3     // Erro grave - algo deu errado
};

/**
 * Classe principal de logging - implementa padrão Singleton
 * 
 * SINGLETON: Só existe uma instância no programa inteiro, acessível globalmente
 * THREAD-SAFE: Múltiplas threads podem usar simultaneamente sem problemas
 * 
 * FUNCIONALIDADES:
 * - Escreve logs em arquivo E no console simultaneamente
 * - Filtragem por nível (DEBUG, INFO, WARNING, ERROR)
 * - Timestamps automáticos
 * - Formatação padronizada das mensagens
 */
class Logger {
public:
    // ========== Padrão Singleton - Impede Cópias ==========
    Logger(const Logger&) = delete;            // Não permite cópia
    Logger& operator=(const Logger&) = delete; // Não permite atribuição
    
    // ========== Acesso à Instância Única ==========
    static Logger& getInstance();  // Retorna a única instância do logger
    
    // ========== Configuração ==========
    bool setLogFile(const std::string& filename);  // Define onde salvar os logs
    void setLevel(LogLevel level);                  // Define nível mínimo (mensagens abaixo são ignoradas)
    
    // ========== Função Principal de Log ==========
    void log(LogLevel level, const std::string& message, const std::string& component = "");
    
    // ========== Funções de Conveniência para Cada Nível ==========
    void debug(const std::string& message, const std::string& component = "");    // LOG DEBUG
    void info(const std::string& message, const std::string& component = "");     // LOG INFO
    void warning(const std::string& message, const std::string& component = "");  // LOG WARNING
    void error(const std::string& message, const std::string& component = "");    // LOG ERROR
    
    // ========== Limpeza ==========
    void close();  // Fecha o arquivo de log e limpa recursos

private:
    // ========== Construtor/Destrutor Privados (Singleton) ==========
    Logger() = default;  // Construtor privado - só getInstance() pode criar
    ~Logger();           // Destrutor - fecha arquivo automaticamente
    
    // ========== Funções Auxiliares ==========
    std::string levelToString(LogLevel level) const;  // Converte enum para string ("DEBUG", "ERROR", etc)
    std::string getCurrentTimestamp() const;           // Gera timestamp atual "[2024-01-01 12:00:00]"
    std::string formatMessage(LogLevel level, const std::string& message, const std::string& component) const;

private:
    // ========== Variáveis de Estado ==========
    std::mutex mutex_;                        // Mutex para thread safety
    std::ofstream logFile_;                   // Stream para arquivo de log
    LogLevel currentLevel_ = LogLevel::INFO;  // Nível mínimo atual (padrão: INFO)
    bool consoleOutput_ = true;               // Se também imprime no console
};

/**
 * ========== MACROS DE CONVENIÊNCIA ==========
 * Facilitam o uso do logger - ao invés de Logger::getInstance().info(...), usar LOG_INFO(...)
 * 
 * USO: LOG_INFO("Sistema iniciado", "MAIN") 
 *      LOG_ERROR("Falha na conexão", "SOCKET_MANAGER")
 */
#define LOG_DEBUG(msg, comp) ipc_project::Logger::getInstance().debug(msg, comp)     // Debug detalhado
#define LOG_INFO(msg, comp) ipc_project::Logger::getInstance().info(msg, comp)       // Informação geral  
#define LOG_WARNING(msg, comp) ipc_project::Logger::getInstance().warning(msg, comp) // Aviso importante
#define LOG_ERROR(msg, comp) ipc_project::Logger::getInstance().error(msg, comp)     // Erro crítico

/**
 * ========== MACROS AINDA MAIS SIMPLES (SEM COMPONENTE) ==========
 * Para uso rápido quando não precisar especificar o componente
 * 
 * USO: LOG_I("Sistema funcionando")
 *      LOG_E("Erro grave!")
 */
#define LOG_D(msg) LOG_DEBUG(msg, "")    // Debug simples
#define LOG_I(msg) LOG_INFO(msg, "")     // Info simples
#define LOG_W(msg) LOG_WARNING(msg, "")  // Warning simples
#define LOG_E(msg) LOG_ERROR(msg, "")    // Error simples

} // namespace ipc_project