/**
 * @file shmem_manager.h
 * @brief Shared memory manager for IPC communication between processes
 */

#pragma once

#include <string>
#include <vector>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../common/logger.h"

namespace ipc_project {

/**
 * @brief Data structure to store shared memory information for frontend display
 * 
 * This structure contains all the necessary information about shared memory
 * operations and status that needs to be sent to the web frontend for visualization.
 */
struct SharedMemoryData {
    std::string content;                    // Current content stored in shared memory
    size_t size;                           // Size of the shared memory segment in bytes
    std::string sync_state;                // Current synchronization state: "locked" or "unlocked" 
    std::vector<pid_t> waiting_processes;  // List of process IDs waiting for memory access
    std::string last_modified;             // ISO timestamp of last modification
    std::string operation;                 // Last operation performed: "create", "write", "read", "destroy"
    pid_t process_id;                      // Process ID that performed the operation
    std::string status;                    // Result status: "success" or "error"
    std::string error_message;             // Human-readable error message if operation failed
    double time_ms;                        // Time taken for the operation in milliseconds
    
    std::string toJSON() const;            // Serialize this data to JSON format
    std::string getCurrentTimestamp() const; // Get current time in ISO format
};

/**
 * @brief Internal structure that resides in the shared memory segment
 * 
 * This is the actual data structure that gets mapped into shared memory
 * and is accessible by all processes. It contains both the user data
 * and synchronization metadata.
 */
struct SharedMemorySegment {
    char data[1024];                       // User data storage (null-terminated string)
    pid_t last_writer;                     // Process ID of the last writer
    time_t last_modified;                  // Unix timestamp of last modification
    int reader_count;                      // Number of processes currently reading
    bool is_writing;                       // True if a writer currently holds the lock
};

/**
 * @brief High-level shared memory manager with reader-writer synchronization
 * 
 * This class implements a robust shared memory system using System V IPC
 * primitives (shmget, shmat) combined with semaphores for thread-safe
 * reader-writer access patterns. It automatically handles:
 * 
 * - Memory segment creation and attachment
 * - Semaphore-based synchronization (readers-writers problem)
 * - Process cleanup and resource management
 * - Error handling and recovery
 * - Cross-process communication
 * 
 * The synchronization follows the classic readers-writers pattern:
 * - Multiple readers can access simultaneously
 * - Writers get exclusive access
 * - Readers are blocked while a writer is active
 * - Writers are blocked while any readers are active
 */
class SharedMemoryManager {
public:
    SharedMemoryManager();
    ~SharedMemoryManager();

    // Basic shared memory operations
    bool createSharedMemory(key_t key = IPC_PRIVATE);  // Create segment
    bool attachToMemory(key_t key);                    // Attach to existing segment
    bool writeMessage(const std::string& message);     // Write to memory
    std::string readMessage();                         // Read from memory
    void destroySharedMemory();                        // Remove segment
    
    // Synchronization operations
    bool lockForWrite();                               // Lock for exclusive write
    bool lockForRead();                                // Lock for shared read
    bool unlock();                                     // Release lock
    
    // Monitoring and status
    SharedMemoryData getLastOperation() const;         // Last operation data
    void printJSON() const;                            // Print JSON to stdout
    bool isActive() const;                             // If active
    key_t getKey() const;                              // Segment key
    
    // Multi-process operations
    bool forkAndTest();                                // Create child process for testing
    bool isParent() const;                             // If this is the parent process
    void waitForChild();                               // Wait for child process to finish

private:
    int shmid_;                            // Shared memory segment ID
    int semid_;                            // Semaphore set ID
    SharedMemorySegment* shared_segment_;  // Pointer to mapped segment
    key_t shm_key_;                        // Shared memory key
    bool is_creator_;                      // If this process created the segment
    bool is_attached_;                     // If attached to segment
    bool is_parent_;                       // If this is the parent process
    pid_t child_pid_;                      // Child process PID
    
    SharedMemoryData last_operation_;      // Last operation data
    Logger& logger_;                       // Logger for debugging
    
    /**
     * @brief Semaphore indices for the semaphore set
     * 
     * We use a set of 3 semaphores to implement the readers-writers synchronization:
     * - SEM_MUTEX: General purpose mutex (currently unused but reserved)
     * - SEM_READER_MUTEX: Protects the reader_count variable
     * - SEM_WRITE: Controls exclusive access for writers and blocks new readers when a writer is waiting
     */
    static const int SEM_MUTEX = 0;       // General mutex semaphore (reserved)
    static const int SEM_READER_MUTEX = 1; // Protects reader_count modifications
    static const int SEM_WRITE = 2;        // Exclusive write access control
    static const int SEM_COUNT = 3;        // Total number of semaphores in the set
    
    // Helper operations
    bool createSemaphores();               // Create semaphore set
    bool attachToSemaphores();             // Attach to existing semaphores
    bool semaphoreOp(int sem_num, int op); // Generic semaphore operation
    void semaphoreWait(int sem_num);       // P(semaphore) - decrement
    void semaphoreSignal(int sem_num);     // V(semaphore) - increment
    
    double getCurrentTimeMs() const;       // Get current time in ms
    std::string getCurrentTimestamp() const; // Formatted timestamp
    void updateOperation(const std::string& op, const std::string& status, 
                        const std::string& error = "");
    
    // Cleanup
    void cleanup();                        // Clean up resources
};

} // namespace ipc_project