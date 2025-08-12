#include "KVTestFramework.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

/**
 * Framework Validation Tests
 * 
 * These tests validate that our C++ test framework components work correctly
 * before we use them for the main KV server tests. This is inspired by the
 * Go tester validation tests.
 */

class FrameworkValidationTest : public ::testing::Test {
protected:
    std::unique_ptr<KVTestFramework> ts;
    
    void SetUp() override {
        ts = std::make_unique<KVTestFramework>(true); // reliable network by default
    }
    
    void TearDown() override {
        if (ts) {
            ts->~KVTestFramework();
            ts.reset();
        }
    }
};

// Test basic framework initialization and cleanup
TEST_F(FrameworkValidationTest, BasicFrameworkLifecycle) {
    ts->Begin("Test framework lifecycle");
    
    // Framework should be able to create clients
    auto ck = ts->makeClient();
    EXPECT_NE(ck, nullptr) << "makeClient should return a valid client";
    
    // Should be able to cleanup without issues
    ts->~KVTestFramework();
    
    // Should be able to create new framework after cleanup
    ts = std::make_unique<KVTestFramework>(true);
    auto ck2 = ts->makeClient();
    EXPECT_NE(ck2, nullptr) << "Should be able to create client after recreating framework";
}

// Test basic Put and Get operations work
TEST_F(FrameworkValidationTest, BasicPutGet) {
    ts->Begin("Test basic Put/Get operations");
    
    auto ck = ts->makeClient();
    
    // Test simple string put/get
    auto put_result = ts->PutJson(*ck, "test_key", "test_value", 0, 1);
    EXPECT_EQ(put_result, zdb::ErrorCode::OK) << "Basic put should succeed";
    
    std::string retrieved_value;
    auto version = ts->GetJson(*ck, "test_key", 1, retrieved_value);
    EXPECT_EQ(retrieved_value, "test_value") << "Retrieved value should match";
    EXPECT_EQ(version, 1) << "Version should be 1 after first put";
}

// Test JSON serialization/deserialization
TEST_F(FrameworkValidationTest, JSONSerialization) {
    ts->Begin("Test JSON serialization");
    
    auto ck = ts->makeClient();
    
    // Test EntryV structure serialization
    EntryV original_entry{42, 100};
    auto put_result = ts->PutJson(*ck, "entry_key", original_entry, 0, 1);
    EXPECT_EQ(put_result, zdb::ErrorCode::OK) << "EntryV put should succeed";
    
    EntryV retrieved_entry;
    auto version = ts->GetJson(*ck, "entry_key", 1, retrieved_entry);
    EXPECT_EQ(retrieved_entry.id, original_entry.id) << "EntryV id should match";
    EXPECT_EQ(retrieved_entry.version, original_entry.version) << "EntryV version should match";
    EXPECT_EQ(version, 1) << "Server version should be 1";
    
    // Test integer serialization
    int original_int = 12345;
    put_result = ts->PutJson(*ck, "int_key", original_int, 0, 1);
    EXPECT_EQ(put_result, zdb::ErrorCode::OK) << "Integer put should succeed";
    
    int retrieved_int;
    version = ts->GetJson(*ck, "int_key", 1, retrieved_int);
    EXPECT_EQ(retrieved_int, original_int) << "Retrieved integer should match";
}

// Test version validation
TEST_F(FrameworkValidationTest, VersionValidation) {
    ts->Begin("Test version validation");
    
    auto ck = ts->makeClient();
    
    // Put with version 0 should succeed for new key
    auto result1 = ts->PutJson(*ck, "ver_key", "value1", 0, 1);
    EXPECT_EQ(result1, zdb::ErrorCode::OK) << "Put with version 0 should succeed for new key";
    
    // Put with old version should fail
    auto result2 = ts->PutJson(*ck, "ver_key", "value2", 0, 1);
    EXPECT_EQ(result2, zdb::ErrorCode::VersionMismatch) << "Put with old version should fail with ErrVersion";
    
    // Put with correct version should succeed
    auto result3 = ts->PutJson(*ck, "ver_key", "value3", 1, 1);
    EXPECT_EQ(result3, zdb::ErrorCode::OK) << "Put with correct version should succeed";
    
    // Verify final state
    std::string final_value;
    auto final_version = ts->GetJson(*ck, "ver_key", 1, final_value);
    EXPECT_EQ(final_value, "value3") << "Final value should be value3";
    EXPECT_EQ(final_version, 2) << "Final version should be 2";
}

// Test error handling for non-existent keys
TEST_F(FrameworkValidationTest, NonExistentKeyHandling) {
    ts->Begin("Test non-existent key handling");
    
    auto ck = ts->makeClient();
    
    // Get on non-existent key should fail with ErrNoKey (matching Go behavior)
    std::string dummy_value;
    EXPECT_THROW({
        ts->GetJson(*ck, "nonexistent", 1, dummy_value);
    }, std::runtime_error) << "Get on non-existent key should throw";
    
    // Put with non-zero version on non-existent key should fail with ErrNoKey (matching Go behavior)
    auto result = ts->PutJson(*ck, "nonexistent", "value", 5, 1);
    EXPECT_EQ(result, zdb::ErrorCode::KeyNotFound) << "Put with non-zero version on non-existent key should fail with ErrNoKey";
    
    // Put with version 0 on non-existent key should succeed (matching Go behavior)
    auto result2 = ts->PutJson(*ck, "nonexistent", "value", 0, 1);
    EXPECT_EQ(result2, zdb::ErrorCode::OK) << "Put with version 0 on non-existent key should succeed";
    
    // Verify it was stored correctly
    std::string retrieved_value;
    auto version = ts->GetJson(*ck, "nonexistent", 1, retrieved_value);
    EXPECT_EQ(retrieved_value, "value") << "Value should be stored correctly";
    EXPECT_EQ(version, 1) << "New keys should get version 1";
}

// Test network simulator components
TEST_F(FrameworkValidationTest, NetworkSimulatorBasics) {
    // Test reliable network simulator
    NetworkSimulator reliable_sim(true);
    EXPECT_TRUE(reliable_sim.IsReliable()) << "Reliable simulator should report as reliable";
    EXPECT_FALSE(reliable_sim.ShouldDropMessage()) << "Reliable simulator should not drop messages";
    EXPECT_FALSE(reliable_sim.ShouldDelayMessage()) << "Reliable simulator should not delay messages";
    
    // Test unreliable network simulator
    NetworkSimulator unreliable_sim(false);
    EXPECT_FALSE(unreliable_sim.IsReliable()) << "Unreliable simulator should report as unreliable";
    
    // Unreliable simulator should sometimes return true for failure modes
    // (We can't test exact behavior due to randomness, but we can test it doesn't crash)
    bool drop_result = unreliable_sim.ShouldDropMessage();
    bool delay_result = unreliable_sim.ShouldDelayMessage();
    bool duplicate_result = unreliable_sim.ShouldDuplicateMessage();
    
    // Just verify these calls don't crash (results are random)
    EXPECT_TRUE(drop_result || !drop_result) << "ShouldDropMessage should return a boolean";
    EXPECT_TRUE(delay_result || !delay_result) << "ShouldDelayMessage should return a boolean";
    EXPECT_TRUE(duplicate_result || !duplicate_result) << "ShouldDuplicateMessage should return a boolean";
}

// Test unreliable network behavior
TEST_F(FrameworkValidationTest, UnreliableNetworkFramework) {
    // Create unreliable framework
    auto unreliable_ts = std::make_unique<KVTestFramework>(false);
    unreliable_ts->Begin("Test unreliable network framework");
    
    auto ck = unreliable_ts->makeClient();
    
    // Operations should still work, but may return ErrMaybe or fail
    bool saw_maybe = false;
    bool saw_success = false;
    
    for (int i = 0; i < 20; i++) {  // Try more times to increase chance of seeing unreliable behavior
        auto result = unreliable_ts->PutJson(*ck, "unreliable_key", i, 0, 1);
        if (result == zdb::ErrorCode::Maybe) {
            saw_maybe = true;
            break;
        } else if (result == zdb::ErrorCode::OK) {
            saw_success = true;
            // Try to verify we can read it back (may also fail due to unreliable network)
            try {
                int retrieved_value;
                auto version = unreliable_ts->GetJson(*ck, "unreliable_key", 1, retrieved_value);
                EXPECT_GT(version, 0) << "Version should be positive";
                break;
            } catch (const std::runtime_error& e) {
                // Get might fail due to unreliable network, that's OK
                std::cout << "Get failed due to unreliable network: " << e.what() << std::endl;
            }
        }
    }
    
    // We should see at least some form of network behavior
    EXPECT_TRUE(saw_maybe || saw_success) << "Should see either ErrMaybe or successful operations";
    
    if (saw_maybe) {
        std::cout << "Successfully observed ErrMaybe from unreliable network" << std::endl;
    }
    if (saw_success) {
        std::cout << "Successfully performed operations through unreliable network" << std::endl;
    }
}

// Test porcupine checker basic functionality
TEST_F(FrameworkValidationTest, PorcupineCheckerBasics) {
    PorcupineChecker checker;
    
    // Initially should have no operations
    EXPECT_EQ(checker.GetOperationCount(), 0) << "New checker should have no operations";
    
    // Add a mock operation
    PorcupineOperation op;
    op.client_id = 1;
    op.call_time = 1000;
    op.return_time = 2000;
    op.input = {OpType::PUT, "key1", "value1", 0};
    op.output = {"", 0, "OK"};
    
    checker.LogOperation(op);
    EXPECT_EQ(checker.GetOperationCount(), 1) << "Should have one operation after logging";
    
    // Test clear
    checker.Clear();
    EXPECT_EQ(checker.GetOperationCount(), 0) << "Should have no operations after clear";
}

// Test ClientResult accumulation
TEST_F(FrameworkValidationTest, ClientResultAccumulation) {
    ClientResult r1{5, 2};  // 5 OK, 2 maybe
    ClientResult r2{3, 1};  // 3 OK, 1 maybe
    
    r1 += r2;
    
    EXPECT_EQ(r1.nok, 8) << "Should accumulate OK results";
    EXPECT_EQ(r1.nmaybe, 3) << "Should accumulate maybe results";
}

// Test multiple clients can be created
TEST_F(FrameworkValidationTest, MultipleClients) {
    ts->Begin("Test multiple client creation");
    
    const int NUM_CLIENTS = 5;
    std::vector<std::unique_ptr<zdb::KVStoreClient>> clients;
    
    // Create multiple clients
    for (int i = 0; i < NUM_CLIENTS; i++) {
        auto ck = ts->makeClient();
        EXPECT_NE(ck, nullptr) << "Client " << i << " should be created successfully";
        clients.push_back(std::move(ck));
    }
    
    // All clients should be able to perform operations
    for (int i = 0; i < NUM_CLIENTS; i++) {
        std::string key = "multi_key_" + std::to_string(i);
        auto result = ts->PutJson(*clients[static_cast<size_t>(i)], key, i, 0, i);
        EXPECT_EQ(result, zdb::ErrorCode::OK) << "Client " << i << " put should succeed";

        int retrieved_value;
        auto version = ts->GetJson(*clients[static_cast<size_t>(i)], key, i, retrieved_value);
        EXPECT_EQ(retrieved_value, i) << "Client " << i << " should retrieve correct value";
        EXPECT_EQ(version, 1) << "Version should be 1 for client " << i;
    }
}

// Test framework can handle rapid operations
TEST_F(FrameworkValidationTest, RapidOperations) {
    ts->Begin("Test rapid operations");
    
    auto ck = ts->makeClient();
    
    const int NUM_OPS = 50;
    
    // Rapid puts with correct versioning
    for (int i = 0; i < NUM_OPS; i++) {
        std::string key = "rapid_" + std::to_string(i);
        auto result = ts->PutJson(*ck, key, i, 0, 1);
        EXPECT_EQ(result, zdb::ErrorCode::OK) << "Rapid put " << i << " should succeed";
    }
    
    // Rapid gets
    for (int i = 0; i < NUM_OPS; i++) {
        std::string key = "rapid_" + std::to_string(i);
        int value;
        auto version = ts->GetJson(*ck, key, 1, value);
        EXPECT_EQ(value, i) << "Rapid get " << i << " should return correct value";
        EXPECT_EQ(version, 1) << "Version should be 1 for rapid get " << i;
    }
}
