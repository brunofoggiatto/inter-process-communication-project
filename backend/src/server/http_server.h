/**
 * @file http_server.h
 * @brief Servidor HTTP para interface web do sistema IPC
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include "../ipc/ipc_coordinator.h"
#include "../common/logger.h"

namespace ipc_project {

// Estrutura pra requisições HTTP
struct HTTPRequest {
    std::string method;          // GET, POST, PUT, DELETE
    std::string path;            // /ipc/status, /ipc/start/pipes, etc
    std::string body;            // corpo da requisição (JSON)
    std::map<std::string, std::string> headers;  // cabeçalhos
    std::map<std::string, std::string> params;   // parâmetros da URL
    
    std::string getParam(const std::string& key, const std::string& default_val = "") const;
};

// Estrutura pra respostas HTTP
struct HTTPResponse {
    int status_code;             // 200, 404, 500, etc
    std::string content_type;    // "application/json", "text/html"
    std::string body;            // conteúdo da resposta
    std::map<std::string, std::string> headers;  // cabeçalhos extras
    
    HTTPResponse(int code = 200, const std::string& type = "application/json");
    void setJSON(const std::string& json_content);
    void setError(int code, const std::string& message);
    std::string toString() const;
};

// Tipo pra handlers de rotas
using RouteHandler = std::function<HTTPResponse(const HTTPRequest&)>;

// Classe principal do servidor HTTP
// Fornece API REST pra controlar o sistema IPC via web
class HTTPServer {
public:
    HTTPServer(int port = 8080);
    ~HTTPServer();

    // Controle do servidor
    bool start();                        // Inicia o servidor
    void stop();                         // Para o servidor
    bool isRunning() const;              // Se tá rodando
    int getPort() const;                 // Porta configurada
    
    // Configuração
    void setPort(int port);              // Define porta
    void setCORS(bool enable);           // Habilita/desabilita CORS
    void setStaticPath(const std::string& path);  // Diretório de arquivos estáticos
    
    // Integração com IPC
    void setIPCCoordinator(std::shared_ptr<IPCCoordinator> coordinator);
    
    // Monitoramento
    size_t getRequestCount() const;      // Total de requisições processadas
    std::vector<std::string> getAccessLogs(size_t count = 50);  // Logs de acesso

private:
    int port_;
    std::atomic<bool> is_running_;
    std::atomic<bool> shutdown_requested_;
    bool cors_enabled_;
    std::string static_path_;
    
    // IPC integration
    std::shared_ptr<IPCCoordinator> coordinator_;
    
    // Servidor interno
    std::unique_ptr<std::thread> server_thread_;
    int server_socket_;
    
    // Estatísticas
    std::atomic<size_t> request_count_;
    std::vector<std::string> access_logs_;
    
    Logger& logger_;
    
    // Thread principal do servidor
    void serverLoop();
    
    // Processamento de requisições
    void handleClient(int client_socket);
    HTTPRequest parseRequest(const std::string& raw_request);
    std::string buildResponse(const HTTPResponse& response);
    
    // Handlers das rotas IPC
    HTTPResponse handleIPCStatus(const HTTPRequest& request);
    HTTPResponse handleIPCStart(const HTTPRequest& request);
    HTTPResponse handleIPCStop(const HTTPRequest& request);
    HTTPResponse handleIPCSend(const HTTPRequest& request);
    HTTPResponse handleIPCLogs(const HTTPRequest& request);
    
    // Handlers gerais
    HTTPResponse handleNotFound(const HTTPRequest& request);
    HTTPResponse handleOptions(const HTTPRequest& request);  // Para CORS
    HTTPResponse handleStaticFile(const HTTPRequest& request);
    
    // Roteamento
    HTTPResponse routeRequest(const HTTPRequest& request);
    bool matchRoute(const std::string& pattern, const std::string& path, 
                    std::map<std::string, std::string>& params);
    
    // Utilidades
    std::string extractPathParam(const std::string& path, const std::string& pattern, 
                                const std::string& param_name);
    std::string urlDecode(const std::string& str);
    std::string getMimeType(const std::string& file_extension);
    void addCORSHeaders(HTTPResponse& response);
    void logRequest(const HTTPRequest& request, const HTTPResponse& response);
    
    // Socket helpers
    bool createSocket();
    void closeSocket();
    std::string readFromSocket(int socket);
    bool writeToSocket(int socket, const std::string& data);
};

// Classe pra servidor WebSocket (pra logs em tempo real)
class WebSocketServer {
public:
    WebSocketServer(int port = 8081);
    ~WebSocketServer();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    // Envia mensagem pra todos os clientes conectados
    void broadcast(const std::string& message);
    
    // Integração com IPC pra streaming de logs
    void setIPCCoordinator(std::shared_ptr<IPCCoordinator> coordinator);

private:
    int port_;
    std::atomic<bool> is_running_;
    std::shared_ptr<IPCCoordinator> coordinator_;
    
    // Clientes conectados
    std::vector<int> connected_clients_;
    std::thread server_thread_;
    std::thread log_streaming_thread_;
    
    Logger& logger_;
    
    void serverLoop();
    void logStreamingLoop();
    void handleWebSocketClient(int client_socket);
    bool performWebSocketHandshake(int client_socket);
    std::string encodeWebSocketFrame(const std::string& message);
    std::string decodeWebSocketFrame(const std::string& frame);
};

// Estrutura pra configuração completa do servidor
struct ServerConfig {
    int http_port = 8080;
    int websocket_port = 8081;
    bool cors_enabled = true;
    std::string static_path = "./frontend/dist";
    bool log_requests = true;
    size_t max_request_size = 1024 * 1024;  // 1MB
    
    void fromJSON(const std::string& json);
    std::string toJSON() const;
};

// Classe helper pra inicializar tudo junto
class WebServerManager {
public:
    WebServerManager(const ServerConfig& config = ServerConfig());
    ~WebServerManager();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    void setIPCCoordinator(std::shared_ptr<IPCCoordinator> coordinator);
    
    HTTPServer& getHTTPServer() { return *http_server_; }
    WebSocketServer& getWebSocketServer() { return *websocket_server_; }

private:
    ServerConfig config_;
    std::unique_ptr<HTTPServer> http_server_;
    std::unique_ptr<WebSocketServer> websocket_server_;
    Logger& logger_;
};

} // namespace ipc_project