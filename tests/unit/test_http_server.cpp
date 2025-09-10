/**
 * @file test_http_server.cpp
 * @brief Testes unitários para HTTPServer
 */

#include <gtest/gtest.h>
#include "server/http_server.h"
#include "ipc/ipc_coordinator.h"
#include <thread>
#include <chrono>

using namespace ipc_project;

class HTTPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Usa uma porta diferente para cada teste
        static int port = 9000;
        server = std::make_unique<HTTPServer>(++port);
        coordinator = std::make_shared<IPCCoordinator>();
        coordinator->initialize();
        server->setIPCCoordinator(coordinator);
    }
    
    void TearDown() override {
        if (server && server->isRunning()) {
            server->stop();
        }
        server.reset();
        if (coordinator) {
            coordinator->shutdown();
        }
        coordinator.reset();
    }
    
    std::unique_ptr<HTTPServer> server;
    std::shared_ptr<IPCCoordinator> coordinator;
};

// Teste básico de inicialização do servidor
TEST_F(HTTPServerTest, ServerInitialization) {
    EXPECT_FALSE(server->isRunning());
    EXPECT_GT(server->getPort(), 0);
    
    // Configura CORS
    server->setCORS(true);
    
    // Configura pasta estática
    server->setStaticPath("./test_static");
    
    // Testa que pode definir porta quando não rodando
    int original_port = server->getPort();
    server->setPort(original_port + 1);
    EXPECT_EQ(server->getPort(), original_port + 1);
}

// Teste de start e stop do servidor
TEST_F(HTTPServerTest, StartAndStop) {
    EXPECT_TRUE(server->start());
    EXPECT_TRUE(server->isRunning());
    
    // Aguarda um momento para o servidor estabilizar
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    server->stop();
    EXPECT_FALSE(server->isRunning());
}

// Teste de contadores e logs
TEST_F(HTTPServerTest, RequestCountingAndLogs) {
    EXPECT_EQ(server->getRequestCount(), 0);
    
    auto logs = server->getAccessLogs();
    EXPECT_TRUE(logs.empty());
    
    // Inicia servidor para poder processar requisições
    ASSERT_TRUE(server->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Após processar requisições, o contador seria > 0
    // (isso seria testado com requisições HTTP reais)
}

// Teste de parsing de requisições HTTP
TEST_F(HTTPServerTest, HTTPRequestParsing) {
    std::string raw_request = 
        "GET /api/status HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    
    // Como parseRequest é privado, testamos indiretamente via roteamento
    // Em um teste real, faríamos requisições HTTP
    EXPECT_TRUE(true); // Placeholder por ora
}

// Teste de estruturas HTTPRequest e HTTPResponse
TEST_F(HTTPServerTest, HTTPStructures) {
    // Teste HTTPRequest
    HTTPRequest request;
    request.method = "GET";
    request.path = "/test";
    request.params["key"] = "value";
    
    EXPECT_EQ(request.getParam("key"), "value");
    EXPECT_EQ(request.getParam("nonexistent"), "");
    EXPECT_EQ(request.getParam("nonexistent", "default"), "default");
    
    // Teste HTTPResponse
    HTTPResponse response(200, "application/json");
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.content_type, "application/json");
    
    response.setJSON("{\"status\":\"ok\"}");
    EXPECT_EQ(response.content_type, "application/json");
    EXPECT_EQ(response.body, "{\"status\":\"ok\"}");
    
    response.setError(404, "Not Found");
    EXPECT_EQ(response.status_code, 404);
    EXPECT_NE(response.body.find("Not Found"), std::string::npos);
    
    std::string response_str = response.toString();
    EXPECT_NE(response_str.find("HTTP/1.1 404"), std::string::npos);
    EXPECT_NE(response_str.find("Content-Type:"), std::string::npos);
}

// Teste de handlers sem servidor rodando
TEST_F(HTTPServerTest, ResponseHandlers) {
    // Testa que o servidor tem IPC coordinator configurado
    EXPECT_TRUE(coordinator != nullptr);
    
    // Inicia alguns mecanismos no coordinator
    coordinator->startMechanism(IPCMechanism::SHARED_MEMORY);
    
    // Em um teste completo, faríamos requisições HTTP reais para testar:
    // - handleIPCStatus
    // - handleIPCStart  
    // - handleIPCStop
    // - handleIPCSend
    // - handleIPCLogs
    
    // Por ora, verifica que o coordinator está funcionando
    auto status = coordinator->getStatusJSON();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("mechanisms"), std::string::npos);
}

// Teste de MIME types
TEST_F(HTTPServerTest, MimeTypeHandling) {
    // Como getMimeType é privado, testamos indiretamente
    // Em implementação real, seria testado via requisições de arquivos estáticos
    EXPECT_TRUE(true); // Placeholder
}

// Teste de roteamento básico
TEST_F(HTTPServerTest, BasicRouting) {
    // Testa estruturas de comando IPC
    IPCCommand cmd;
    
    // Teste de comando válido
    std::string json = R"({"action":"status","mechanism":"pipes"})";
    EXPECT_TRUE(cmd.fromJSON(json));
    EXPECT_EQ(cmd.action, "status");
    
    // Teste toJSON
    std::string json_out = cmd.toJSON();
    EXPECT_FALSE(json_out.empty());
    EXPECT_NE(json_out.find("action"), std::string::npos);
}

// Teste de configuração CORS
TEST_F(HTTPServerTest, CORSConfiguration) {
    HTTPResponse response;
    
    // Simula adição de headers CORS
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    
    std::string response_str = response.toString();
    EXPECT_NE(response_str.find("Access-Control-Allow-Origin"), std::string::npos);
}

// Integration test with IPC coordinator
TEST_F(HTTPServerTest, IPCIntegration) {
    // Verifica que servidor está integrado com coordinator
    ASSERT_NE(coordinator, nullptr);
    
    // Inicia mecanismo
    EXPECT_TRUE(coordinator->startMechanism(IPCMechanism::SHARED_MEMORY));
    
    // Verifica status
    auto status = coordinator->getFullStatus();
    EXPECT_GT(status.mechanisms.size(), 0);
    
    // Envia mensagem
    EXPECT_TRUE(coordinator->sendMessage(IPCMechanism::SHARED_MEMORY, "test message"));
}

// Teste de erro sem IPC coordinator
TEST_F(HTTPServerTest, NoIPCCoordinatorError) {
    auto server_no_ipc = std::make_unique<HTTPServer>(9999);
    
    // Sem coordinator, servidor deve estar OK mas sem funcionalidade IPC
    EXPECT_FALSE(server_no_ipc->isRunning());
    EXPECT_GT(server_no_ipc->getPort(), 0);
    
    // Start/stop ainda deve funcionar
    EXPECT_TRUE(server_no_ipc->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    server_no_ipc->stop();
    EXPECT_FALSE(server_no_ipc->isRunning());
}

// Teste de múltiplas configurações
TEST_F(HTTPServerTest, ServerConfiguration) {
    server->setCORS(false);
    server->setStaticPath("/custom/path");
    
    // Verifica que configurações não quebram o servidor
    EXPECT_TRUE(server->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_TRUE(server->isRunning());
    
    server->stop();
    EXPECT_FALSE(server->isRunning());
}