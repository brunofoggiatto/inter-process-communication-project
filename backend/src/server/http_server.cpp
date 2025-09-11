/**
 * @file http_server.cpp
 * @brief Implementação completa do servidor HTTP para interface web do sistema IPC
 * 
 * Este arquivo implementa um servidor HTTP completo do zero, incluindo:
 * - Socket server TCP/IP nativo (sem bibliotecas externas)
 * - Parser de requisições HTTP (GET, POST, OPTIONS)
 * - Sistema de roteamento para API REST
 * - Integração com IPCCoordinator para controle remoto
 * - Suporte a CORS para desenvolvimento frontend
 * - Servidor de arquivos estáticos (HTML, CSS, JS)
 * - Serialização/deserialização JSON automática
 * 
 * ARQUITETURA DO SERVIDOR:
 * Cliente HTTP → Socket TCP → Parser HTTP → Router → Handler → Response HTTP
 *                                                        ↓
 *                                           IPCCoordinator → IPC Managers
 * 
 * ENDPOINTS IMPLEMENTADOS:
 * GET  /ipc/status           - Status geral do sistema
 * POST /ipc/start/{mechanism} - Inicia mecanismo (pipes|sockets|shmem)
 * POST /ipc/stop/{mechanism}  - Para mecanismo
 * POST /ipc/send/{mechanism}  - Envia mensagem via mecanismo
 * GET  /ipc/logs/{mechanism}  - Obtém logs do mecanismo
 * GET  /*                     - Arquivos estáticos (frontend)
 */

#include "http_server.h"  // Header da classe
#include <sys/socket.h>   // Para socket(), bind(), listen()
#include <netinet/in.h>   // Para struct sockaddr_in
#include <arpa/inet.h>    // Para inet_addr()
#include <unistd.h>       // Para close(), read(), write()
#include <fcntl.h>        // Para fcntl() - configuração de socket
#include <sstream>        // Para construção de strings
#include <fstream>        // Para leitura de arquivos estáticos
#include <algorithm>      // Para std::find e manipulação de strings
#include <cstring>        // Para strcmp(), strlen()

namespace ipc_project {

// ============================================================================
// IMPLEMENTAÇÃO DAS ESTRUTURAS HTTP (Request/Response)
// ============================================================================

/**
 * Obtém parâmetro da URL com valor padrão
 * @param key Nome do parâmetro
 * @param default_val Valor padrão se não encontrado
 * @return Valor do parâmetro ou valor padrão
 */
std::string HTTPRequest::getParam(const std::string& key, const std::string& default_val) const {
    auto it = params.find(key);
    return (it != params.end()) ? it->second : default_val;
}

// ============================================================================
// IMPLEMENTAÇÃO DA CLASSE HTTPResponse
// ============================================================================
HTTPResponse::HTTPResponse(int code, const std::string& type) 
    : status_code(code), content_type(type) {
}

void HTTPResponse::setJSON(const std::string& json_content) {
    content_type = "application/json";
    body = json_content;
}

void HTTPResponse::setError(int code, const std::string& message) {
    status_code = code;
    content_type = "application/json";
    body = "{\"error\":\"" + message + "\",\"code\":" + std::to_string(code) + "}";
}

std::string HTTPResponse::toString() const {
    std::stringstream response;
    
    // Status line
    response << "HTTP/1.1 " << status_code;
    switch (status_code) {
        case 200: response << " OK"; break;
        case 404: response << " Not Found"; break;
        case 500: response << " Internal Server Error"; break;
        case 400: response << " Bad Request"; break;
        default: response << " Unknown"; break;
    }
    response << "\r\n";
    
    // Headers
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Connection: close\r\n";
    
    // Headers extras
    for (const auto& header : headers) {
        response << header.first << ": " << header.second << "\r\n";
    }
    
    response << "\r\n";
    response << body;
    
    return response.str();
}

// Implementação HTTPServer
HTTPServer::HTTPServer(int port) 
    : port_(port), is_running_(false), shutdown_requested_(false), 
      cors_enabled_(true), server_socket_(-1), request_count_(0),
      logger_(Logger::getInstance()) {
    
    logger_.info("HTTPServer criado na porta " + std::to_string(port), "HTTP");
}

HTTPServer::~HTTPServer() {
    stop();
}

bool HTTPServer::start() {
    if (is_running_) {
        logger_.warning("Servidor já está rodando", "HTTP");
        return true;
    }
    
    if (!createSocket()) {
        return false;
    }
    
    is_running_ = true;
    shutdown_requested_ = false;
    server_thread_ = std::make_unique<std::thread>(&HTTPServer::serverLoop, this);
    
    logger_.info("Servidor HTTP iniciado na porta " + std::to_string(port_), "HTTP");
    return true;
}

void HTTPServer::stop() {
    if (!is_running_) return;
    
    logger_.info("Parando servidor HTTP...", "HTTP");
    shutdown_requested_ = true;
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    closeSocket();
    is_running_ = false;
    logger_.info("Servidor HTTP parado", "HTTP");
}

bool HTTPServer::isRunning() const {
    return is_running_;
}

int HTTPServer::getPort() const {
    return port_;
}

void HTTPServer::setPort(int port) {
    if (!is_running_) {
        port_ = port;
    }
}

void HTTPServer::setCORS(bool enable) {
    cors_enabled_ = enable;
}

void HTTPServer::setStaticPath(const std::string& path) {
    static_path_ = path;
}

void HTTPServer::setIPCCoordinator(std::shared_ptr<IPCCoordinator> coordinator) {
    coordinator_ = coordinator;
}

size_t HTTPServer::getRequestCount() const {
    return request_count_;
}

std::vector<std::string> HTTPServer::getAccessLogs(size_t count) {
    size_t start = access_logs_.size() > count ? access_logs_.size() - count : 0;
    return std::vector<std::string>(access_logs_.begin() + start, access_logs_.end());
}

bool HTTPServer::createSocket() {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        logger_.error("Falha ao criar socket: " + std::string(strerror(errno)), "HTTP");
        return false;
    }
    
    // Permite reutilizar endereço
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configura endereço
    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    // Bind
    if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        logger_.error("Falha no bind: " + std::string(strerror(errno)), "HTTP");
        close(server_socket_);
        return false;
    }
    
    // Listen
    if (listen(server_socket_, 10) < 0) {
        logger_.error("Falha no listen: " + std::string(strerror(errno)), "HTTP");
        close(server_socket_);
        return false;
    }
    
    return true;
}

void HTTPServer::closeSocket() {
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
}

void HTTPServer::serverLoop() {
    while (!shutdown_requested_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);
        
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            logger_.error("Erro no select: " + std::string(strerror(errno)), "HTTP");
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_socket_, &read_fds)) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket >= 0) {
                std::thread client_thread(&HTTPServer::handleClient, this, client_socket);
                client_thread.detach();
            }
        }
    }
}

void HTTPServer::handleClient(int client_socket) {
    std::string raw_request = readFromSocket(client_socket);
    if (raw_request.empty()) {
        close(client_socket);
        return;
    }
    
    HTTPRequest request = parseRequest(raw_request);
    HTTPResponse response = routeRequest(request);
    
    if (cors_enabled_) {
        addCORSHeaders(response);
    }
    
    std::string response_str = response.toString();
    writeToSocket(client_socket, response_str);
    
    logRequest(request, response);
    request_count_++;
    
    close(client_socket);
}

HTTPRequest HTTPServer::parseRequest(const std::string& raw_request) {
    HTTPRequest request;
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parse primeira linha (método e path)
    if (std::getline(stream, line)) {
        std::istringstream first_line(line);
        first_line >> request.method >> request.path;
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 2); // pula ': '
            if (value.back() == '\r') value.pop_back();
            request.headers[key] = value;
        }
    }
    
    // Parse body se houver
    std::string body;
    while (std::getline(stream, line)) {
        body += line + "\n";
    }
    if (!body.empty()) {
        body.pop_back(); // remove último \n
        request.body = body;
    }
    
    return request;
}

HTTPResponse HTTPServer::routeRequest(const HTTPRequest& request) {
    // OPTIONS para CORS
    if (request.method == "OPTIONS") {
        return handleOptions(request);
    }
    
    // Rotas IPC
    if (request.path == "/ipc/status" && request.method == "GET") {
        return handleIPCStatus(request);
    }
    
    // Start mechanism: POST /ipc/start/{mechanism}
    std::map<std::string, std::string> params;
    if (matchRoute("/ipc/start/*", request.path, params) && request.method == "POST") {
        HTTPRequest modified_request = request;
        modified_request.params = params;
        return handleIPCStart(modified_request);
    }
    
    // Stop mechanism: POST /ipc/stop/{mechanism}
    if (matchRoute("/ipc/stop/*", request.path, params) && request.method == "POST") {
        HTTPRequest modified_request = request;
        modified_request.params = params;
        return handleIPCStop(modified_request);
    }
    
    // Send message: POST /ipc/send
    if (request.path == "/ipc/send" && request.method == "POST") {
        return handleIPCSend(request);
    }
    
    // Get logs: GET /ipc/logs/{mechanism}
    if (matchRoute("/ipc/logs/*", request.path, params) && request.method == "GET") {
        HTTPRequest modified_request = request;
        modified_request.params = params;
        return handleIPCLogs(modified_request);
    }

    // Get detail: GET /ipc/detail/{mechanism}
    if (matchRoute("/ipc/detail/*", request.path, params) && request.method == "GET") {
        HTTPRequest modified_request = request;
        modified_request.params = params;
        return handleIPCDetail(modified_request);
    }
    
    // Arquivos estáticos
    if (request.method == "GET" && !static_path_.empty()) {
        return handleStaticFile(request);
    }
    
    return handleNotFound(request);
}

HTTPResponse HTTPServer::handleIPCStatus(const HTTPRequest& request) {
    if (!coordinator_) {
        HTTPResponse response;
        response.setError(503, "IPC Coordinator not available");
        return response;
    }
    
    HTTPResponse response;
    response.setJSON(coordinator_->getStatusJSON());
    return response;
}

HTTPResponse HTTPServer::handleIPCStart(const HTTPRequest& request) {
    if (!coordinator_) {
        HTTPResponse response;
        response.setError(503, "IPC Coordinator not available");
        return response;
    }
    
    std::string mechanism = request.getParam("0"); // primeiro parâmetro capturado
    IPCMechanism mech;
    
    if (mechanism == "pipes") {
        mech = IPCMechanism::PIPES;
    } else if (mechanism == "sockets") {
        mech = IPCMechanism::SOCKETS;
    } else if (mechanism == "shmem" || mechanism == "shared_memory") {
        mech = IPCMechanism::SHARED_MEMORY;
    } else {
        HTTPResponse response;
        response.setError(400, "Invalid mechanism: " + mechanism);
        return response;
    }
    
    bool success = coordinator_->startMechanism(mech);
    
    HTTPResponse response;
    if (success) {
        response.setJSON("{\"status\":\"success\",\"message\":\"" + mechanism + " started\"}");
    } else {
        response.setError(500, "Failed to start " + mechanism);
    }
    
    return response;
}

HTTPResponse HTTPServer::handleIPCStop(const HTTPRequest& request) {
    if (!coordinator_) {
        HTTPResponse response;
        response.setError(503, "IPC Coordinator not available");
        return response;
    }
    
    std::string mechanism = request.getParam("0");
    IPCMechanism mech;
    
    if (mechanism == "pipes") {
        mech = IPCMechanism::PIPES;
    } else if (mechanism == "sockets") {
        mech = IPCMechanism::SOCKETS;
    } else if (mechanism == "shmem" || mechanism == "shared_memory") {
        mech = IPCMechanism::SHARED_MEMORY;
    } else {
        HTTPResponse response;
        response.setError(400, "Invalid mechanism: " + mechanism);
        return response;
    }
    
    bool success = coordinator_->stopMechanism(mech);
    
    HTTPResponse response;
    if (success) {
        response.setJSON("{\"status\":\"success\",\"message\":\"" + mechanism + " stopped\"}");
    } else {
        response.setError(500, "Failed to stop " + mechanism);
    }
    
    return response;
}

HTTPResponse HTTPServer::handleIPCSend(const HTTPRequest& request) {
    if (!coordinator_) {
        HTTPResponse response;
        response.setError(503, "IPC Coordinator not available");
        return response;
    }
    
    // Parse JSON do body (parsing manual simples)
    std::string mechanism_str;
    std::string message;
    
    // Extrai mechanism do JSON
    size_t mech_pos = request.body.find("\"mechanism\":\"");
    if (mech_pos != std::string::npos) {
        mech_pos += 13; // tamanho de "mechanism":"
        size_t end_pos = request.body.find("\"", mech_pos);
        if (end_pos != std::string::npos) {
            mechanism_str = request.body.substr(mech_pos, end_pos - mech_pos);
        }
    }
    
    // Extrai message do JSON
    size_t msg_pos = request.body.find("\"message\":\"");
    if (msg_pos != std::string::npos) {
        msg_pos += 11; // tamanho de "message":"
        size_t end_pos = request.body.find("\"", msg_pos);
        if (end_pos != std::string::npos) {
            message = request.body.substr(msg_pos, end_pos - msg_pos);
        }
    }
    
    if (mechanism_str.empty() || message.empty()) {
        HTTPResponse response;
        response.setError(400, "Missing mechanism or message in request body");
        return response;
    }
    
    IPCMechanism mech;
    if (mechanism_str == "pipes") {
        mech = IPCMechanism::PIPES;
    } else if (mechanism_str == "sockets") {
        mech = IPCMechanism::SOCKETS;
    } else if (mechanism_str == "shmem" || mechanism_str == "shared_memory") {
        mech = IPCMechanism::SHARED_MEMORY;
    } else {
        HTTPResponse response;
        response.setError(400, "Invalid mechanism: " + mechanism_str);
        return response;
    }
    
    bool success = coordinator_->sendMessage(mech, message);
    
    HTTPResponse response;
    if (success) {
        response.setJSON("{\"status\":\"success\",\"message\":\"Message sent via " + mechanism_str + "\"}");
    } else {
        response.setError(500, "Failed to send message via " + mechanism_str);
    }
    
    return response;
}

HTTPResponse HTTPServer::handleIPCDetail(const HTTPRequest& request) {
    if (!coordinator_) {
        HTTPResponse response;
        response.setError(503, "IPC Coordinator not available");
        return response;
    }

    std::string mechanism = request.getParam("0");
    IPCMechanism mech;
    if (mechanism == "pipes") mech = IPCMechanism::PIPES;
    else if (mechanism == "sockets") mech = IPCMechanism::SOCKETS;
    else if (mechanism == "shmem" || mechanism == "shared_memory") mech = IPCMechanism::SHARED_MEMORY;
    else {
        HTTPResponse response;
        response.setError(400, "Invalid mechanism: " + mechanism);
        return response;
    }

    HTTPResponse response;
    response.setJSON(coordinator_->getMechanismDetailJSON(mech));
    return response;
}

HTTPResponse HTTPServer::handleIPCLogs(const HTTPRequest& request) {
    if (!coordinator_) {
        HTTPResponse response;
        response.setError(503, "IPC Coordinator not available");
        return response;
    }
    
    std::string mechanism = request.getParam("0");
    IPCMechanism mech;
    
    if (mechanism == "pipes") {
        mech = IPCMechanism::PIPES;
    } else if (mechanism == "sockets") {
        mech = IPCMechanism::SOCKETS;
    } else if (mechanism == "shmem" || mechanism == "shared_memory") {
        mech = IPCMechanism::SHARED_MEMORY;
    } else {
        HTTPResponse response;
        response.setError(400, "Invalid mechanism: " + mechanism);
        return response;
    }
    
    auto logs = coordinator_->getLogs(mech, 100);
    
    std::string json_logs = "[";
    for (size_t i = 0; i < logs.size(); ++i) {
        if (i > 0) json_logs += ",";
        json_logs += "\"" + logs[i] + "\"";
    }
    json_logs += "]";
    
    HTTPResponse response;
    response.setJSON("{\"mechanism\":\"" + mechanism + "\",\"logs\":" + json_logs + "}");
    return response;
}

HTTPResponse HTTPServer::handleNotFound(const HTTPRequest& request) {
    HTTPResponse response;
    response.setError(404, "Endpoint not found: " + request.method + " " + request.path);
    return response;
}

HTTPResponse HTTPServer::handleOptions(const HTTPRequest& request) {
    HTTPResponse response(200);
    response.body = "";
    return response;
}

HTTPResponse HTTPServer::handleStaticFile(const HTTPRequest& request) {
    std::string file_path = static_path_ + request.path;
    if (request.path == "/") {
        file_path += "index.html";
    }
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return handleNotFound(request);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    HTTPResponse response;
    response.body = content;
    
    // Determina MIME type
    size_t dot = file_path.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = file_path.substr(dot);
        response.content_type = getMimeType(ext);
    }
    
    return response;
}

bool HTTPServer::matchRoute(const std::string& pattern, const std::string& path, 
                           std::map<std::string, std::string>& params) {
    // Implementação simples de matching com wildcards
    if (pattern.find('*') == std::string::npos) {
        return pattern == path;
    }
    
    size_t star_pos = pattern.find('*');
    std::string prefix = pattern.substr(0, star_pos);
    
    if (path.size() < prefix.size() || path.substr(0, prefix.size()) != prefix) {
        return false;
    }
    
    // Captura o que vem depois do prefixo
    std::string captured = path.substr(prefix.size());
    params["0"] = captured;
    
    return true;
}

std::string HTTPServer::getMimeType(const std::string& file_extension) {
    if (file_extension == ".html" || file_extension == ".htm") return "text/html";
    if (file_extension == ".css") return "text/css";
    if (file_extension == ".js") return "application/javascript";
    if (file_extension == ".json") return "application/json";
    if (file_extension == ".png") return "image/png";
    if (file_extension == ".jpg" || file_extension == ".jpeg") return "image/jpeg";
    if (file_extension == ".gif") return "image/gif";
    if (file_extension == ".svg") return "image/svg+xml";
    return "text/plain";
}

void HTTPServer::addCORSHeaders(HTTPResponse& response) {
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    // Evita cache para respostas da API
    response.headers["Cache-Control"] = "no-store, no-cache, must-revalidate";
    response.headers["Pragma"] = "no-cache";
}

void HTTPServer::logRequest(const HTTPRequest& request, const HTTPResponse& response) {
    std::string log_entry = request.method + " " + request.path + " " + 
                           std::to_string(response.status_code);
    access_logs_.push_back(log_entry);
    
    // Mantém apenas os últimos 1000 logs
    if (access_logs_.size() > 1000) {
        access_logs_.erase(access_logs_.begin());
    }
    
    logger_.info(log_entry, "HTTP");
}

std::string HTTPServer::readFromSocket(int socket) {
    // Lê cabeçalhos primeiro
    std::string data;
    char buffer[4096];
    while (true) {
        ssize_t bytes = recv(socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        data.append(buffer, buffer + bytes);
        // Checa fim de headers
        auto pos = data.find("\r\n\r\n");
        if (pos != std::string::npos) {
            // Descobre Content-Length
            std::string headers = data.substr(0, pos + 4);
            size_t cl_pos = headers.find("Content-Length:");
            size_t content_length = 0;
            if (cl_pos != std::string::npos) {
                cl_pos += std::string("Content-Length:").size();
                // Pula possíveis espaços
                while (cl_pos < headers.size() && (headers[cl_pos] == ' ' || headers[cl_pos] == '\t')) cl_pos++;
                size_t end = headers.find("\r\n", cl_pos);
                if (end != std::string::npos) {
                    content_length = static_cast<size_t>(std::stol(headers.substr(cl_pos, end - cl_pos)));
                }
            }

            size_t have_body = data.size() - (pos + 4);
            while (have_body < content_length) {
                ssize_t more = recv(socket, buffer, sizeof(buffer), 0);
                if (more <= 0) break;
                data.append(buffer, buffer + more);
                have_body += static_cast<size_t>(more);
            }
            break;
        }
        // Proteção: evita loop infinito em requisições sem headers completos
        if (data.size() > 1'000'000) break; // 1MB
    }
    return data;
}

bool HTTPServer::writeToSocket(int socket, const std::string& data) {
    size_t total_sent = 0;
    while (total_sent < data.length()) {
        ssize_t sent = send(socket, data.c_str() + total_sent, 
                           data.length() - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += sent;
    }
    return true;
}

} // namespace ipc_project
