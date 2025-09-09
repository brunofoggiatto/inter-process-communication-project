/**
 * @file pipe_manager.cpp
 * @brief Implementacao de pipes anonimos pra comunicacao IPC
 */

#include "pipe_manager.h"
#include <iostream>
#include <cstring>
#include <errno.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>

namespace ipc_project {

// Converte nossa estrutura de dados pro formato JSON do frontend
std::string PipeData::toJSON() const {
    std::ostringstream json;
    json << "{"
         << "\"message\":\"" << message << "\","
         << "\"bytes\":" << bytes << ","
         << "\"time_ms\":" << std::fixed << std::setprecision(3) << time_ms << ","
         << "\"status\":\"" << status << "\","
         << "\"sender_pid\":" << sender_pid << ","
         << "\"receiver_pid\":" << receiver_pid << ","
         << "\"ipc_type\":\"anonymous_pipe\""
         // TODO: adicionar mais campos? tipo timestamp absoluto?
         << "}";
    return json.str();
}

// Construtor - configura estado inicial mas nao cria o pipe ainda
PipeManager::PipeManager() 
    : child_pid_(-1),
      is_parent_(true),
      is_active_(false),
      logger_(Logger::getInstance()) {
    
    // Inicializa file descriptors como invalidos
    pipe_fd_[0] = -1;
    pipe_fd_[1] = -1;
    
    // TODO: talvez seja melhor inicializar isso depois?
    // configura dados da operacao inicial
    last_operation_.message = "";
    last_operation_.bytes = 0;
    last_operation_.time_ms = 0.0;
    last_operation_.status = "idle";
    last_operation_.sender_pid = getpid();
    last_operation_.receiver_pid = -1;
    
    logger_.info("PipeManager created", "PIPE");
}

// destrutor - garante que tudo seja limpo corretamente
PipeManager::~PipeManager() {
    closePipe();
    logger_.debug("PipeManager destroyed", "PIPE");
}

// Funcao principal pra criar o pipe e dar fork nos processos pai/filho  
bool PipeManager::createPipe() {
    logger_.info("Creating anonymous pipe", "PIPE");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Cria o pipe - isso nos da dois file descriptors
    if (pipe(pipe_fd_) == -1) {
        updateOperation("", 0, "error_create");
        logger_.error("Failed to create pipe: " + std::string(strerror(errno)), "PIPE");
        return false;
    }
    
    // Fork cria uma copia do nosso processo
    child_pid_ = fork();
    
    if (child_pid_ == -1) {
        // fork falhou - limpa o pipe
        close(pipe_fd_[0]);
        close(pipe_fd_[1]);
        updateOperation("", 0, "error_fork");
        logger_.error("Failed to fork: " + std::string(strerror(errno)), "PIPE");
        return false;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (child_pid_ == 0) {
        // Estamos no processo filho agora - so recebe mensagens
        is_parent_ = false;
        close(pipe_fd_[1]); // Fecha lado de escrita - filho nao manda
        pipe_fd_[1] = -1;
        
        last_operation_.sender_pid = getppid(); // pid do pai
        last_operation_.receiver_pid = getpid(); // Nosso PID
        
        logger_.info("Child process created", "PIPE_CHILD");
        
        // Loop para manter o processo filho rodando e esperando mensagens
        runChildLoop();
    } else {
        // estamos no processo pai - so manda mensagens
        is_parent_ = true;
        close(pipe_fd_[0]); // Fecha lado de leitura - pai nao recebe
        pipe_fd_[0] = -1;
        
        last_operation_.sender_pid = getpid(); // nosso PID  
        last_operation_.receiver_pid = child_pid_; // pid do filho
        
        logger_.info("Parent process - child PID: " + std::to_string(child_pid_), "PIPE");
    }
    
    is_active_ = true;
    updateOperation("pipe_created", 0, "ready");
    last_operation_.time_ms = elapsed;
    
    return true;
}

bool PipeManager::isParent() const {
    return is_parent_;
}

// Manda mensagem pelo pipe (so processo pai consegue fazer isso)
bool PipeManager::sendMessage(const std::string& message) {
    if (!is_active_ || !is_parent_ || pipe_fd_[1] == -1) {
        updateOperation(message, 0, "error_invalid_state");
        logger_.error("Attempt to write to invalid pipe", "PIPE");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Adiciona newline pra ficar mais facil de ler pro filho
    std::string msg_final = message + "\n";  // mudei o nome pra ficar mais simples
    // std::string msg_with_newline = message + "\n";  // nome antigo era muito longo
    
    // escreve a mensagem no pipe
    ssize_t bytes_written = write(pipe_fd_[1], msg_final.c_str(), msg_final.length());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (bytes_written == -1) {
        updateOperation(message, 0, "error_write");
        logger_.error("Error writing to pipe: " + std::string(strerror(errno)), "PIPE");
        return false;
    }
    
    // atualiza nosso tracking de operacao
    updateOperation(message, static_cast<size_t>(bytes_written), "sent");
    last_operation_.time_ms = elapsed;
    
    logger_.info("Sent: '" + message + "' (" + std::to_string(bytes_written) + " bytes)", "PIPE");
    
    // Manda JSON pro stdout pra frontend ver o que aconteceu
    printJSON();
    
    return true;
}

// recebe mensagem do pipe (so processo filho consegue)
std::string PipeManager::receiveMessage() {
    if (!is_active_ || is_parent_ || pipe_fd_[0] == -1) {
        updateOperation("", 0, "error_invalid_state");
        logger_.error("Attempt to read from invalid pipe", "PIPE_CHILD");
        return "";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // buffer pra guardar a mensagem que vamos receber
    char buf[1024];  // nome mais curto, mais comum em C
    ssize_t bytes_read = read(pipe_fd_[0], buf, sizeof(buf) - 1);  // mudei pra buf
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    if (bytes_read == -1) {
        updateOperation("", 0, "error_read");
        logger_.error("Error reading from pipe: " + std::string(strerror(errno)), "PIPE_CHILD");
        return "";
    }
    
    if (bytes_read == 0) {
        // pai fechou o pipe - nao vem mais mensagem
        updateOperation("", 0, "eof");
        logger_.info("EOF received - pipe closed by parent", "PIPE_CHILD");
        return "";
    }
    
    // garante que a string termina direito
    buf[bytes_read] = '\0';
    std::string msg_recebida(buf);  // nome em portugues mesmo
    
    // remove o newline que o pai colocou
    if (!msg_recebida.empty() && msg_recebida.back() == '\n') {
        msg_recebida.pop_back();
    }
    
    // atualiza nosso tracking de operacao
    updateOperation(msg_recebida, static_cast<size_t>(bytes_read), "received");
    last_operation_.time_ms = elapsed;
    
    logger_.info("Received: '" + msg_recebida + "' (" + std::to_string(bytes_read) + " bytes)", "PIPE_CHILD");
    
    // manda JSON pro stdout pra frontend ver o que aconteceu
    printJSON();
    
    return msg_recebida;
}

PipeData PipeManager::getLastOperation() const {
    return last_operation_;
}

// Imprime dados da operacao como JSON pro stdout pra frontend monitorar
void PipeManager::printJSON() const {
    // Prefixo especial pro frontend saber que eh dado do pipe
    std::cout << "PIPE_JSON:" << last_operation_.toJSON() << std::endl;
    std::cout.flush();
}

// limpa o pipe e espera o processo filho terminar
void PipeManager::closePipe() {
    if (!is_active_) {
        return; // ja fechou
    }
    
    logger_.info("Closing pipe", is_parent_ ? "PIPE" : "PIPE_CHILD");
    
    if (is_parent_) {
        // limpeza do processo pai
        if (pipe_fd_[1] != -1) {
            close(pipe_fd_[1]); // fecha lado de escrita
            pipe_fd_[1] = -1;
        }
        
        // espera o processo filho terminar
        if (child_pid_ > 0) {
            int status;
            logger_.debug("Waiting for child process to terminate", "PIPE");
            waitpid(child_pid_, &status, 0);
            // TODO: talvez usar WNOHANG pra nao ficar travado?
            
            if (WIFEXITED(status)) {
                logger_.info("Child process terminated with code: " + 
                           std::to_string(WEXITSTATUS(status)), "PIPE");
            }
            // if (WIFSIGNALED(status)) {  // codigo comentado - talvez usar depois
            //     logger_.warning("Child killed by signal", "PIPE");
            // }
        }
    } else {
        // limpeza do processo filho
        if (pipe_fd_[0] != -1) {
            close(pipe_fd_[0]); // fecha lado de leitura
            pipe_fd_[0] = -1;
        }
    }
    
    is_active_ = false;
    updateOperation("", 0, "closed");
}

bool PipeManager::isActive() const {
    return is_active_;
}

// funcao auxiliar pra pegar tempo atual em milissegundos
double PipeManager::getCurrentTimeMs() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(epoch).count();
}

// funcao auxiliar pra atualizar dados de tracking das operacoes
void PipeManager::updateOperation(const std::string& msg, size_t bytes, const std::string& status) {
    last_operation_.message = msg;
    last_operation_.bytes = bytes;
    last_operation_.status = status;
    // obs: time_ms vai ser definido pela funcao que chama quando necessario
    // tentei fazer automatico mas ficou complicado
}

// Loop principal do processo filho para receber mensagens
void PipeManager::runChildLoop() {
    logger_.info("Iniciando loop do processo filho", "PIPES");
    
    // Loop infinito aguardando mensagens do processo pai
    while (true) {
        char buf[1024];
        ssize_t bytes_read = read(pipe_fd_[0], buf, sizeof(buf) - 1);
        
        if (bytes_read == -1) {
            logger_.error("Erro na leitura do pipe: " + std::string(strerror(errno)), "PIPE_CHILD");
            break;
        }
        
        if (bytes_read == 0) {
            // Processo pai fechou o pipe
            logger_.info("EOF recebido - processo pai fechou o pipe", "PIPE_CHILD");
            break;
        }
        
        buf[bytes_read] = '\0';
        std::string message(buf);
        
        // Remove newline se existir
        if (!message.empty() && message.back() == '\n') {
            message.pop_back();
        }
        
        if (!message.empty()) {
            logger_.info("Mensagem recebida: " + message, "PIPE_CHILD");
            
            // Atualiza operação e envia JSON
            updateOperation(message, static_cast<size_t>(bytes_read), "received");
            printJSON();
        }
    }
    
    logger_.info("Fechando pipe do processo filho", "PIPE_CHILD");
    if (pipe_fd_[0] != -1) {
        close(pipe_fd_[0]);
    }
    
    // Processo filho termina aqui
    exit(0);
}

} // namespace ipc_project