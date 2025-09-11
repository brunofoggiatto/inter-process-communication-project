/**
 * @file shmem_manager.cpp 
 * @brief Implementação completa do gerenciador de memória compartilhada
 * 
 * Este arquivo implementa todas as operações de memória compartilhada:
 * - Criação/destruição de segmentos System V IPC
 * - Sincronização leitor-escritor com semáforos  
 * - Operações thread-safe de leitura/escrita
 * - Serialização JSON para monitoramento web
 * - Tratamento robusto de erros e limpeza de recursos
 * 
 * SINCRONIZAÇÃO IMPLEMENTADA:
 * - Problema clássico leitores-escritores
 * - Múltiplos leitores simultâneos
 * - Escritores com acesso exclusivo
 * - Prevenção de starvation
 */

#include "shmem_manager.h"  // Header da classe
#include <iostream>         // Para I/O padrão
#include <cstring>          // Para funções de string
#include <chrono>           // Para timestamps
#include <format>           // Para formatação de strings (C++20)
#include <sys/errno.h>      // Para códigos de erro do sistema
#include <signal.h>         // Para tratamento de sinais

namespace ipc_project {

/**
 * Implementação da serialização JSON para SharedMemoryData
 * Converte estrutura interna para formato JSON para dashboard web
 * @return String JSON formatada com todos os dados
 */
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

// Constructor
SharedMemoryManager::SharedMemoryManager() 
    : shmid_(-1), semid_(-1), shared_segment_(nullptr), shm_key_(IPC_PRIVATE),
      is_creator_(false), is_attached_(false), is_parent_(true), child_pid_(-1),
      logger_(Logger::getInstance()) {
    
    logger_.info("SharedMemoryManager created", "SHMEM");
}

// Destructor
SharedMemoryManager::~SharedMemoryManager() {
    cleanup();
    logger_.info("SharedMemoryManager destroyed", "SHMEM");
}

// Create shared memory segment
bool SharedMemoryManager::createSharedMemory(key_t key) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    shm_key_ = (key == IPC_PRIVATE) ? ftok("/tmp", getpid()) : key;
    
    logger_.info(std::format("Creating shared memory with key: {}", shm_key_), "SHMEM");
    
    // Create shared memory segment
    shmid_ = shmget(shm_key_, sizeof(SharedMemorySegment), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid_ == -1) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        std::string error = std::format("Failed to create shared memory: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        updateOperation("create", "error", error);
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    is_creator_ = true;
    
    // Attach to segment
    if (!attachToMemory(shm_key_)) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    // Create semaphores
    if (!createSemaphores()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    // Initialize structure in shared memory safely
    memset(shared_segment_, 0, sizeof(SharedMemorySegment));
    shared_segment_->last_writer = getpid();
    shared_segment_->last_modified = time(nullptr);
    shared_segment_->reader_count = 0;
    shared_segment_->is_writing = false;
    
    // Use safe string copy
    const char* init_msg = "Shared memory initialized";
    strncpy(shared_segment_->data, init_msg, sizeof(shared_segment_->data) - 1);
    shared_segment_->data[sizeof(shared_segment_->data) - 1] = '\0';
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("create", "success");
    last_operation_.time_ms = elapsed;
    last_operation_.content = shared_segment_->data;
    last_operation_.size = sizeof(SharedMemorySegment);
    
    logger_.info("Shared memory created successfully", "SHMEM");
    return true;
}

// Attach to existing segment
bool SharedMemoryManager::attachToMemory(key_t key) {
    shm_key_ = key;
    
    // If we don't have ID, find existing one
    if (shmid_ == -1) {
        shmid_ = shmget(key, sizeof(SharedMemorySegment), 0666);
        if (shmid_ == -1) {
            std::string error = std::format("Failed to find shared memory: {}", strerror(errno));
            logger_.error(error, "SHMEM");
            updateOperation("attach", "error", error);
            return false;
        }
    }
    
    // Attach segment to address space
    shared_segment_ = static_cast<SharedMemorySegment*>(shmat(shmid_, nullptr, 0));
    if (shared_segment_ == (void*)-1) {
        std::string error = std::format("Failed to attach shared memory: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        shared_segment_ = nullptr;
        updateOperation("attach", "error", error);
        return false;
    }
    
    is_attached_ = true;
    
    // Attach to existing semaphores if not creator
    if (!is_creator_ && !attachToSemaphores()) {
        updateOperation("attach", "error", "Failed to attach to semaphores");
        return false;
    }
    
    logger_.info("Attached to shared memory", "SHMEM");
    return true;
}

// Write message to memory
bool SharedMemoryManager::writeMessage(const std::string& message) {
    if (!is_attached_ || !shared_segment_) {
        updateOperation("write", "error", "Not attached to shared memory");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Lock for exclusive write
    if (!lockForWrite()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        updateOperation("write", "error", "Failed to acquire write lock");
        last_operation_.time_ms = elapsed;
        return false;
    }
    
    // Write to memory
    strncpy(shared_segment_->data, message.c_str(), sizeof(shared_segment_->data) - 1);
    shared_segment_->data[sizeof(shared_segment_->data) - 1] = '\0';
    shared_segment_->last_writer = getpid();
    shared_segment_->last_modified = time(nullptr);
    
    // Release lock
    unlock();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("write", "success");
    last_operation_.time_ms = elapsed;
    last_operation_.content = shared_segment_->data;
    
    logger_.info(std::format("Written to memory: {}", message), "SHMEM");
    return true;
}

// Read message from memory
std::string SharedMemoryManager::readMessage() {
    if (!is_attached_ || !shared_segment_) {
        updateOperation("read", "error", "Not attached to shared memory");
        return "";
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Lock for shared read
    if (!lockForRead()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        updateOperation("read", "error", "Failed to acquire read lock");
        last_operation_.time_ms = elapsed;
        return "";
    }
    
    std::string content(shared_segment_->data);
    
    // Release lock
    unlock();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("read", "success");
    last_operation_.time_ms = elapsed;
    last_operation_.content = content;
    
    logger_.info(std::format("Read from memory: {}", content), "SHMEM");
    return content;
}

// Remove shared memory segment
void SharedMemoryManager::destroySharedMemory() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (is_creator_ && shmid_ != -1) {
        // Remove semaphores
        if (semid_ != -1) {
            if (semctl(semid_, 0, IPC_RMID) == -1) {
                logger_.warning(std::format("Failed to remove semaphores: {}", strerror(errno)), "SHMEM");
            } else {
                logger_.info("Semaphores removed", "SHMEM");
            }
        }
        
        // Remove shared memory
        if (shmctl(shmid_, IPC_RMID, nullptr) == -1) {
            auto end_time = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            
            std::string error = std::format("Failed to remove shared memory: {}", strerror(errno));
            logger_.error(error, "SHMEM");
            updateOperation("destroy", "error", error);
            last_operation_.time_ms = elapsed;
            return;
        }
        
        logger_.info("Shared memory removed", "SHMEM");
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateOperation("destroy", "success");
    last_operation_.time_ms = elapsed;
    
    cleanup();
}

/**
 * @brief Acquire exclusive write lock using readers-writers synchronization
 * 
 * This implements the "writer" side of the classic readers-writers problem.
 * A writer must wait until all readers have finished and no other writer
 * is active before it can proceed.
 * 
 * @return true if lock acquired successfully, false on error
 */
bool SharedMemoryManager::lockForWrite() {
    if (semid_ == -1) {
        logger_.error("Cannot acquire write lock: semaphores not initialized", "SHMEM");
        return false;
    }
    
    try {
        // Block until no readers are active and no other writer holds the lock
        // This semaphore starts at 1, so first writer gets it, blocking all others
        semaphoreWait(SEM_WRITE);
        
        // Mark that we're now writing to prevent new readers
        shared_segment_->is_writing = true;
        return true;
    } catch (const std::exception& e) {
        logger_.error(std::format("Write lock error: {}", e.what()), "SHMEM");
        return false;
    }
}

/**
 * @brief Acquire shared read lock allowing multiple concurrent readers
 * 
 * This implements the "reader" side of the readers-writers problem.
 * Multiple readers can access the shared memory simultaneously, but
 * they block writers from accessing until all readers are done.
 * 
 * The algorithm:
 * 1. Acquire mutex to safely modify reader count
 * 2. Increment reader count
 * 3. If this is the first reader, acquire the write semaphore to block writers
 * 4. Release mutex so other readers can proceed
 * 
 * @return true if lock acquired successfully, false on error
 */
bool SharedMemoryManager::lockForRead() {
    if (semid_ == -1) {
        logger_.error("Cannot acquire read lock: semaphores not initialized", "SHMEM");
        return false;
    }
    
    try {
        // Acquire mutex to safely increment reader count
        semaphoreWait(SEM_READER_MUTEX);
        
        // Add ourselves to the reader count
        shared_segment_->reader_count++;
        
        // If we're the first reader, block any writers from proceeding
        if (shared_segment_->reader_count == 1) {
            semaphoreWait(SEM_WRITE);
        }
        
        // Release mutex so other readers can also acquire locks
        semaphoreSignal(SEM_READER_MUTEX);
        return true;
        
    } catch (const std::exception& e) {
        // CRITICAL: If we fail after incrementing reader count, we must fix it
        // Otherwise the system will think there's a reader when there isn't
        try {
            semaphoreWait(SEM_READER_MUTEX);
            if (shared_segment_->reader_count > 0) {
                shared_segment_->reader_count--;
            }
            semaphoreSignal(SEM_READER_MUTEX);
        } catch (...) {
            // Log but don't throw - we're already in error handling
            logger_.error("Failed to clean up reader count after error", "SHMEM");
        }
        logger_.error(std::format("Read lock error: {}", e.what()), "SHMEM");
        return false;
    }
}

/**
 * @brief Release either a read or write lock
 * 
 * This function automatically detects whether the caller held a read or write
 * lock and performs the appropriate cleanup. For writers, it simply releases
 * the write semaphore. For readers, it decrements the reader count and releases
 * the write semaphore only when the last reader exits.
 * 
 * @return true if lock released successfully, false on error
 */
bool SharedMemoryManager::unlock() {
    if (semid_ == -1) {
        logger_.warning("Cannot unlock: semaphores not initialized", "SHMEM");
        return false;
    }
    
    if (!shared_segment_) {
        logger_.warning("Cannot unlock: not attached to shared memory", "SHMEM");
        return false;
    }
    
    try {
        if (shared_segment_->is_writing) {
            // We were a writer - simply release the write lock
            shared_segment_->is_writing = false;
            semaphoreSignal(SEM_WRITE);
            logger_.debug("Released write lock", "SHMEM");
        } else {
            // We were a reader - need to decrement count safely
            semaphoreWait(SEM_READER_MUTEX);
            
            if (shared_segment_->reader_count > 0) {
                shared_segment_->reader_count--;
                
                // If we're the last reader, allow writers to proceed
                if (shared_segment_->reader_count == 0) {
                    semaphoreSignal(SEM_WRITE);
                    logger_.debug("Last reader released write lock", "SHMEM");
                } else {
                    logger_.debug(std::format("Reader released, {} readers remaining", 
                                            shared_segment_->reader_count), "SHMEM");
                }
            } else {
                logger_.warning("Unlock called but no active readers", "SHMEM");
            }
            
            semaphoreSignal(SEM_READER_MUTEX);
        }
        
        return true;
    } catch (const std::exception& e) {
        logger_.error(std::format("Error releasing lock: {}", e.what()), "SHMEM");
        return false;
    }
}

// Create semaphore set
bool SharedMemoryManager::createSemaphores() {
    // Create set of 3 semaphores
    semid_ = semget(shm_key_, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0666);
    if (semid_ == -1) {
        std::string error = std::format("Failed to create semaphores: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        return false;
    }
    
    // Initialize semaphores
    if (semctl(semid_, SEM_MUTEX, SETVAL, 1) == -1 ||
        semctl(semid_, SEM_READER_MUTEX, SETVAL, 1) == -1 ||
        semctl(semid_, SEM_WRITE, SETVAL, 1) == -1) {
        
        std::string error = std::format("Failed to initialize semaphores: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        return false;
    }
    
    logger_.info("Semaphores created and initialized", "SHMEM");
    return true;
}

/**
 * @brief Attach to an existing semaphore set
 * 
 * This function is called when a process wants to attach to shared memory
 * that was created by another process. Instead of creating new semaphores,
 * it finds and attaches to the existing semaphore set.
 * 
 * @return true if successfully attached, false on error
 */
bool SharedMemoryManager::attachToSemaphores() {
    // Don't attach twice
    if (semid_ != -1) {
        logger_.debug("Already attached to semaphores", "SHMEM");
        return true;
    }
    
    // Find existing semaphore set using the same key as shared memory
    semid_ = semget(shm_key_, SEM_COUNT, 0666);
    if (semid_ == -1) {
        std::string error = std::format("Failed to find semaphores: {}", strerror(errno));
        logger_.error(error, "SHMEM");
        return false;
    }
    
    logger_.info("Attached to existing semaphores", "SHMEM");
    return true;
}

/**
 * @brief Perform a semaphore operation with timeout and retry logic
 * 
 * This is the core semaphore operation function that all other semaphore
 * operations use. It includes several safety features:
 * 
 * - Timeout to prevent indefinite blocking (5 seconds)
 * - SEM_UNDO flag to automatically release semaphores if process dies
 * - Retry logic for signal interruptions
 * - Comprehensive error handling
 * 
 * @param sem_num Index of semaphore in the set (0, 1, or 2)
 * @param op Operation to perform: -1 (wait/P), +1 (signal/V), or 0 (test)
 * @return true if operation succeeded, false on timeout or error
 */
bool SharedMemoryManager::semaphoreOp(int sem_num, int op) {
    if (semid_ == -1) {
        logger_.error("Cannot perform semaphore operation: not attached", "SHMEM");
        return false;
    }
    
    // Set up semaphore operation structure
    // SEM_UNDO ensures automatic cleanup if this process dies unexpectedly
    struct sembuf semaphore_operation = {
        .sem_num = static_cast<unsigned short>(sem_num),
        .sem_op = static_cast<short>(op),
        .sem_flg = SEM_UNDO
    };
    
    // Set timeout to prevent deadlocks (5 seconds should be plenty)
    struct timespec timeout = {
        .tv_sec = 5,
        .tv_nsec = 0
    };
    
    // Retry up to 3 times if interrupted by signals
    int max_retries = 3;
    for (int attempt = 0; attempt < max_retries; attempt++) {
        int result = semtimedop(semid_, &semaphore_operation, 1, &timeout);
        
        if (result == 0) {
            // Success!
            return true;
        }
        
        // Handle different types of errors
        if (errno == EINTR) {
            // Interrupted by signal - this is recoverable, try again
            logger_.debug(std::format("Semaphore operation interrupted, retrying (attempt {})", attempt + 1), "SHMEM");
            continue;
        } else if (errno == EAGAIN || errno == ETIMEDOUT) {
            // Timeout - this suggests a deadlock or very slow operation
            logger_.warning(std::format("Semaphore operation timeout for sem[{}], op={}", sem_num, op), "SHMEM");
            return false;
        } else {
            // Other error - probably a programming error or system issue
            logger_.error(std::format("Semaphore operation failed: {}", strerror(errno)), "SHMEM");
            return false;
        }
    }
    
    // If we get here, we've exhausted all retries
    logger_.error("Semaphore operation failed after maximum retries", "SHMEM");
    return false;
}

void SharedMemoryManager::semaphoreWait(int sem_num) {
    if (!semaphoreOp(sem_num, -1)) {
        throw std::runtime_error("Failed semaphore wait operation");
    }
}

void SharedMemoryManager::semaphoreSignal(int sem_num) {
    if (!semaphoreOp(sem_num, 1)) {
        throw std::runtime_error("Failed semaphore signal operation");
    }
}

// Create child process for testing
bool SharedMemoryManager::forkAndTest() {
    if (!is_attached_) {
        logger_.error("Cannot fork without attached memory", "SHMEM");
        return false;
    }
    
    child_pid_ = fork();
    if (child_pid_ == -1) {
        logger_.error(std::format("Fork failed: {}", strerror(errno)), "SHMEM");
        return false;
    }
    
    is_parent_ = (child_pid_ != 0);
    
    if (is_parent_) {
        logger_.info(std::format("Child process created: {}", child_pid_), "SHMEM");
    } else {
        logger_.info("Running as child process", "SHMEM");
    }
    
    return true;
}

void SharedMemoryManager::waitForChild() {
    if (is_parent_ && child_pid_ > 0) {
        int status;
        waitpid(child_pid_, &status, 0);
        logger_.info("Child process finished", "SHMEM");
        child_pid_ = -1;
    }
}

// Utility functions and getters
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
    
    // Update list of waiting processes (simplified)
    last_operation_.waiting_processes.clear();
    if (shared_segment_ && shared_segment_->reader_count > 0) {
        // In real implementation, would maintain list of waiting PIDs
        last_operation_.waiting_processes.push_back(getpid());
    }
}

void SharedMemoryManager::cleanup() {
    // Force unlock any locks we might be holding before cleanup
    if (shared_segment_ && shared_segment_ != (void*)-1) {
        try {
            // Emergency cleanup of any locks we might be holding
            if (shared_segment_->is_writing) {
                shared_segment_->is_writing = false;
                if (semid_ != -1) {
                    semaphoreSignal(SEM_WRITE);
                }
            }
        } catch (...) {
            // Ignore errors during emergency cleanup
        }
    }
    
    // Detach from shared memory
    if (shared_segment_ && shared_segment_ != (void*)-1) {
        if (shmdt(shared_segment_) == -1) {
            logger_.warning(std::format("Failed to detach memory: {}", strerror(errno)), "SHMEM");
        }
        shared_segment_ = nullptr;
    }
    
    is_attached_ = false;
    shmid_ = -1;
    semid_ = -1;
}

// Implementation of getCurrentTimestamp function for SharedMemoryData
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