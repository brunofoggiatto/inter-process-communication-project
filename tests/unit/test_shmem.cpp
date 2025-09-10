/**
 * @file test_shmem.cpp
 * @brief Unit tests for SharedMemoryManager
 */

#include <gtest/gtest.h>
#include "ipc/shmem_manager.h"

using namespace ipc_project;

class SharedMemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<SharedMemoryManager>();
    }
    
    void TearDown() override {
        if (manager) {
            manager->destroySharedMemory();
        }
        manager.reset();
    }
    
    std::unique_ptr<SharedMemoryManager> manager;
};

// Basic test for creation and destruction
TEST_F(SharedMemoryManagerTest, CreateAndDestroy) {
    EXPECT_TRUE(manager->createSharedMemory());
    EXPECT_TRUE(manager->isActive());
    
    manager->destroySharedMemory();
    EXPECT_FALSE(manager->isActive());
}

// Write and read test
TEST_F(SharedMemoryManagerTest, WriteAndRead) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    std::string test_message = "Test message in shared memory";
    EXPECT_TRUE(manager->writeMessage(test_message));
    
    std::string read_message = manager->readMessage();
    EXPECT_EQ(test_message, read_message);
}

// Basic synchronization test
TEST_F(SharedMemoryManagerTest, BasicLocking) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Test write lock
    EXPECT_TRUE(manager->lockForWrite());
    EXPECT_TRUE(manager->unlock());
    
    // Test read lock
    EXPECT_TRUE(manager->lockForRead());
    EXPECT_TRUE(manager->unlock());
}

// JSON operations test
TEST_F(SharedMemoryManagerTest, JSONOperations) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    std::string message = "Test JSON";
    manager->writeMessage(message);
    
    auto last_op = manager->getLastOperation();
    EXPECT_EQ(last_op.operation, "write");
    EXPECT_EQ(last_op.status, "success");
    EXPECT_EQ(last_op.content, message);
    
    std::string json = last_op.toJSON();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("shared_memory"), std::string::npos);
}

// Multiple operations test
TEST_F(SharedMemoryManagerTest, MultipleOperations) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // Write multiple messages
    std::vector<std::string> messages = {
        "First message",
        "Second message", 
        "Third message"
    };
    
    for (const auto& msg : messages) {
        EXPECT_TRUE(manager->writeMessage(msg));
        std::string read_msg = manager->readMessage();
        EXPECT_EQ(msg, read_msg);
    }
}

// Status and monitoring test
TEST_F(SharedMemoryManagerTest, StatusMonitoring) {
    EXPECT_FALSE(manager->isActive());
    
    ASSERT_TRUE(manager->createSharedMemory());
    EXPECT_TRUE(manager->isActive());
    
    key_t key = manager->getKey();
    EXPECT_NE(key, -1);
    
    // Test getLastOperation before any operation
    auto initial_op = manager->getLastOperation();
    EXPECT_EQ(initial_op.operation, "create");
    EXPECT_EQ(initial_op.status, "success");
}

// Error test - try to use without creating
TEST_F(SharedMemoryManagerTest, ErrorHandling) {
    // Try to write without creating first
    EXPECT_FALSE(manager->writeMessage("test"));
    
    // Try to read without creating first  
    std::string result = manager->readMessage();
    EXPECT_TRUE(result.empty());
    
    // Verify last operation has error
    auto last_op = manager->getLastOperation();
    EXPECT_EQ(last_op.status, "error");
}

// Child process test (if applicable)
TEST_F(SharedMemoryManagerTest, ProcessManagement) {
    ASSERT_TRUE(manager->createSharedMemory());
    
    // By default, should be the parent process
    EXPECT_TRUE(manager->isParent());
    
    // We don't test fork here as it can complicate unit tests
    // This would be better tested in integration tests
}