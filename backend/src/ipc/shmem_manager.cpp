/**
 * @file shmem_manager.cpp 
 * @brief Implementação do gerenciador de memória compartilhada
 */

#include "shmem_manager.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <format>
#include <sys/errno.h>
#include <signal.h>

namespace ipc_project {

// Implementação do SharedMemoryData::toJSON()
std::string SharedMemoryData::toJSON() const {
    std::string waiting_pids = "[";
    for (size_t i = 0; i < waiting_processes.size(); ++i) {
        if (i > 0) waiting_pids += ",";
        waiting_pids += std::to_string(waiting_processes[i]);
    }
    waiting_pids += "]";
    
    std::string error_str = error_message.empty() ? "null" : ("\"" + error_message + "\"");
    
    return std::format(R"({{
  "type": "shared_memory",
  "timestamp": "{}",
  "operation": "{}",
  "process_id": {},
  "data": {{
    "content": "{}",
    "size": {},
    "sync_state": "{}",
    "waiting_processes": {},
    "last_modified": "{}"
  }},
  "status": "{}",
  "error_message": {}
}})", getCurrentTimestamp(), operation, process_id, content, size, sync_state, 
     waiting_pids, last_modified, status, error_str);
}

// Construtor
SharedMemoryManager::SharedMemoryManager() 
    : shmid_(-1), semid_(-1), shared_segment_(nullptr), shm_key_(IPC_PRIVATE),
      is_creator_(false), is_attached_(false), is_parent_(true), child_pid_(-1),
      logger_(Logger::getInstance()) {
    
    logger_.info("SharedMemoryManager criado", "SHMEM");
}

// Destrutor
SharedMemoryManager::~SharedMemoryManager() {
    cleanup();
    logger_.info("SharedMemoryManager destruído", "SHMEM");
}

// Cria segmento de memória compartilhada
bool SharedMemoryManager::createSharedMemory(key_t key) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    shm_key_ = (key == IPC_PRIVATE) ? ftok("/tmp", getpid()) : key;
    
    logger_.info(std::format("Criando memória compartilhada com chave: {}", shm_key_), "SHMEM");
    
    // Cria segmento de memória compartilhada
    shmid_ = shmget(shm_key_, sizeof(SharedMemorySegment), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid_ == -1) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        std::string error = std::format("Falha ao criar memória compartilhada: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        updateOperation("create", "error", error);
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    is_creator_ = true;
    
    // Anexa ao segmento
    if (!attachToMemory(shm_key_)) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    // Cria semáforos
    if (!createSemaphores()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    // Inicializa a estrutura na memória compartilhada
    memset(shared_segment_, 0, sizeof(SharedMemorySegment));
    shared_segment_->last_writer = getpid();
    shared_segment_->last_modified = time(nullptr);
    shared_segment_->reader_count = 0;
    shared_segment_->is_writing = false;
    strcpy(shared_segment_->data, "Memória compartilhada inicializada");
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("create", "success");
    last_operation_.time_ms = elapsed;
    last_operation_.content = shared_segment_->data;
    last_operation_.size = sizeof(SharedMemorySegment);
    
    logger_.info("Memória compartilhada criada com sucesso", "SHMEM");
    return true;
}

// Anexa ao segmento existente
bool SharedMemoryManager::attachToMemory(key_t key) {
    shm_key_ = key;
    
    // Se não temos ID, busca pelo existente
    if (shmid_ == -1) {
        shmid_ = shmget(key, sizeof(SharedMemorySegment), 0666);
        if (shmid_ == -1) {
            std::string error = std::format("Falha ao encontrar memória compartilhada: {}", strerror(errno));
            logger_.error(error, "SHMEM");
            updateOperation("attach", "error", error);
            return false;
        }
    }
    
    // Anexa o segmento ao espaço de endereçamento
    shared_segment_ = static_cast<SharedMemorySegment*>(shmat(shmid_, nullptr, 0));
    if (shared_segment_ == (void*)-1) {
        std::string error = std::format("Falha ao anexar memória compartilhada: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        shared_segment_ = nullptr;
        updateOperation("attach", "error", error);
        return false;
    }
    
    is_attached_ = true;
    logger_.info("Anexado à memória compartilhada", "SHMEM");
    return true;
}

// Escreve mensagem na memória
bool SharedMemoryManager::writeMessage(const std::string& message) {
    if (!is_attached_ || !shared_segment_) {
        updateOperation("write", "error", "Não anexado à memória compartilhada");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Trava pra escrita exclusiva
    if (!lockForWrite()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        updateOperation("write", "error", "Falha ao obter lock de escrita");
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    // Escreve na memória
    strncpy(shared_segment_->data, message.c_str(), sizeof(shared_segment_->data) - 1);
    shared_segment_->data[sizeof(shared_segment_->data) - 1] = '\0';
    shared_segment_->last_writer = getpid();
    shared_segment_->last_modified = time(nullptr);
    
    // Libera o lock
    unlock();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("write", "success");
    last_operation_.time_ms = elapsed;
    last_operation_.content = shared_segment_->data;
    
    logger_.info(std::format("Escrito na memória: {}", message), "SHMEM");
    return true;
}

// Lê mensagem da memória
std::string SharedMemoryManager::readMessage() {
    if (!is_attached_ || !shared_segment_) {
        updateOperation("read", "error", "Não anexado à memória compartilhada");
        return "";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Trava pra leitura compartilhada
    if (!lockForRead()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        updateOperation("read", "error", "Falha ao obter lock de leitura");
        last_operation_.time_ms = elapsed;
        return "";
    }
    
    std::string content(shared_segment_->data);
    
    // Libera o lock
    unlock();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("read", "success");
    last_operation_.time_ms = elapsed;
    last_operation_.content = content;
    
    logger_.info(std::format("Lido da memória: {}", content), "SHMEM");
    return content;
}

// Remove o segmento de memória
void SharedMemoryManager::destroySharedMemory() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (is_creator_ && shmid_ != -1) {
        // Remove semáforos
        if (semid_ != -1) {
            if (semctl(semid_, 0, IPC_RMID) == -1) {
                logger_.warning(std::format("Falha ao remover semáforos: {}", strerror(errno)), "SHMEM");
            } else {
                logger_.info("Semáforos removidos", "SHMEM");
            }
        }
        
        // Remove memória compartilhada
        if (shmctl(shmid_, IPC_RMID, nullptr) == -1) {
            auto end_time = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            
            std::string error = std::format("Falha ao remover memória compartilhada: {}", strerror(errno));
            logger_.error(error, "SHMEM");
            updateOperation("destroy", "error", error);
            last_operation_.time_ms = elapsed;
            return;
        }
        
        logger_.info("Memória compartilhada removida", "SHMEM");
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("destroy", "success");
    last_operation_.time_ms = elapsed;
    
    cleanup();
}

// Trava pra escrita exclusiva (readers-writers problem)
bool SharedMemoryManager::lockForWrite() {
    if (semid_ == -1) return false;
    
    try {
        // Espera que não haja leitores nem escritores
        semaphoreWait(SEM_WRITE);
        shared_segment_->is_writing = true;
        return true;
    } catch (const std::exception& e) {
        logger_.error(std::format("Erro no lock de escrita: {}", e.what()), "SHMEM");
        return false;
    }
}

// Trava pra leitura compartilhada
bool SharedMemoryManager::lockForRead() {
    if (semid_ == -1) return false;
    
    try {
        // Protege o contador de leitores
        semaphoreWait(SEM_READER_MUTEX);
        
        shared_segment_->reader_count++;
        if (shared_segment_->reader_count == 1) {
            // Primeiro leitor bloqueia escritores
            semaphoreWait(SEM_WRITE);
        }
        
        semaphoreSignal(SEM_READER_MUTEX);
        return true;
    } catch (const std::exception& e) {
        logger_.error(std::format("Erro no lock de leitura: {}", e.what()), "SHMEM");
        return false;
    }
}

// Libera o lock
bool SharedMemoryManager::unlock() {
    if (semid_ == -1 || !shared_segment_) return false;
    
    try {
        if (shared_segment_->is_writing) {
            // Era um escritor
            shared_segment_->is_writing = false;
            semaphoreSignal(SEM_WRITE);
        } else if (shared_segment_->reader_count > 0) {
            // Era um leitor
            semaphoreWait(SEM_READER_MUTEX);
            
            shared_segment_->reader_count--;
            if (shared_segment_->reader_count == 0) {
                // Último leitor libera escritores
                semaphoreSignal(SEM_WRITE);
            }
            
            semaphoreSignal(SEM_READER_MUTEX);
        }
        
        return true;
    } catch (const std::exception& e) {
        logger_.error(std::format("Erro ao liberar lock: {}", e.what()), "SHMEM");
        return false;
    }
}

// Cria conjunto de semáforos
bool SharedMemoryManager::createSemaphores() {
    // Cria conjunto de 3 semáforos
    semid_ = semget(shm_key_, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0666);
    if (semid_ == -1) {
        std::string error = std::format("Falha ao criar semáforos: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        return false;
    }
    
    // Inicializa semáforos
    if (semctl(semid_, SEM_MUTEX, SETVAL, 1) == -1 ||
        semctl(semid_, SEM_READER_MUTEX, SETVAL, 1) == -1 ||
        semctl(semid_, SEM_WRITE, SETVAL, 1) == -1) {
        
        std::string error = std::format("Falha ao inicializar semáforos: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        return false;
    }
    
    logger_.info("Semáforos criados e inicializados", "SHMEM");
    return true;
}

// Operação genérica em semáforo
bool SharedMemoryManager::semaphoreOp(int sem_num, int op) {
    if (semid_ == -1) return false;
    
    struct sembuf sb = {static_cast<unsigned short>(sem_num), static_cast<short>(op), 0};
    
    if (semop(semid_, &sb, 1) == -1) {
        if (errno != EINTR) {
            logger_.error(std::format("Operação semáforo falhou: {}", strerror(errno)), "SHMEM");
            return false;
        }
        // Se foi interrompido por sinal, tenta novamente
        return semaphoreOp(sem_num, op);
    }
    
    return true;
}

void SharedMemoryManager::semaphoreWait(int sem_num) {
    if (!semaphoreOp(sem_num, -1)) {
        throw std::runtime_error("Falha na operação wait do semáforo");
    }
}

void SharedMemoryManager::semaphoreSignal(int sem_num) {
    if (!semaphoreOp(sem_num, 1)) {
        throw std::runtime_error("Falha na operação signal do semáforo");
    }
}

// Cria processo filho pra testar
bool SharedMemoryManager::forkAndTest() {
    if (!is_attached_) {
        logger_.error("Não é possível fazer fork sem memória anexada", "SHMEM");
        return false;
    }
    
    child_pid_ = fork();
    if (child_pid_ == -1) {
        logger_.error(std::format("Falha no fork: {}", strerror(errno)), "SHMEM");
        return false;
    }
    
    is_parent_ = (child_pid_ != 0);
    
    if (is_parent_) {
        logger_.info(std::format("Processo filho criado: {}", child_pid_), "SHMEM");
    } else {
        logger_.info("Executando como processo filho", "SHMEM");
    }
    
    return true;
}

void SharedMemoryManager::waitForChild() {
    if (is_parent_ && child_pid_ > 0) {
        int status;
        waitpid(child_pid_, &status, 0);
        logger_.info("Processo filho terminou", "SHMEM");
        child_pid_ = -1;
    }
}

// Funções de utilidade e getters
SharedMemoryData SharedMemoryManager::getLastOperation() const {
    return last_operation_;
}

void SharedMemoryManager::printJSON() const {
    std::cout << last_operation_.toJSON() << std::endl;
}

bool SharedMemoryManager::isActive() const {
    return is_attached_ && shared_segment_ != nullptr;
}

key_t SharedMemoryManager::getKey() const {
    return shm_key_;
}

bool SharedMemoryManager::isParent() const {
    return is_parent_;
}

double SharedMemoryManager::getCurrentTimeMs() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(duration).count();
}

std::string SharedMemoryManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm* tm = std::gmtime(&time_t);
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec, ms.count());
}

void SharedMemoryManager::updateOperation(const std::string& op, const std::string& status, const std::string& error) {
    last_operation_.operation = op;
    last_operation_.status = status;
    last_operation_.error_message = error;
    last_operation_.process_id = getpid();
    last_operation_.sync_state = (shared_segment_ && shared_segment_->is_writing) ? "locked" : "unlocked";
    last_operation_.last_modified = getCurrentTimestamp();
    
    // Atualiza lista de processos esperando (simplificado)
    last_operation_.waiting_processes.clear();
    if (shared_segment_ && shared_segment_->reader_count > 0) {
        // Em implementação real, manteria lista de PIDs esperando
        last_operation_.waiting_processes.push_back(getpid());
    }
}

void SharedMemoryManager::cleanup() {
    // Desanexa da memória compartilhada
    if (shared_segment_ && shared_segment_ != (void*)-1) {
        if (shmdt(shared_segment_) == -1) {
            logger_.warning(std::format("Falha ao desanexar memória: {}", strerror(errno)), "SHMEM");
        }
        shared_segment_ = nullptr;
    }
    
    is_attached_ = false;
    shmid_ = -1;
    semid_ = -1;
}

// Implementação da função getCurrentTimestamp para SharedMemoryData
std::string SharedMemoryData::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm* tm = std::gmtime(&time_t);
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec, ms.count());
}

} // namespace ipc_project