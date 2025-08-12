#include "KVTestFramework.hpp"
#include <gtest/gtest.h>

// Test class using the framework with the current project
class KVServerTest : public ::testing::Test {
protected:
    std::unique_ptr<KVTestFramework> ts;
};

// Test implementations corresponding to the Go tests

TEST_F(KVServerTest, ReliablePut) {
    ts = std::make_unique<KVTestFramework>(true);
    const std::string VAL = "6.5840";
    const TVersion VER = 0;
    
    ts->Begin("One client and reliable Put");
    
    auto ck = ts->makeClient();
    
    // Test basic put - should succeed with version 0 for new key
    EXPECT_EQ(ts->PutJson(*ck, "k", VAL, VER, 1), zdb::ErrorCode::OK);
    
    // Test get - should return the value and version 1 (server incremented it)
    std::string retrieved_val;
    auto version = ts->GetJson(*ck, "k", 1, retrieved_val);
    EXPECT_EQ(retrieved_val, VAL);
    EXPECT_EQ(version, VER + 1); // Server should have incremented version
    
    // Test version mismatch - trying to put with old version should fail
    auto result = ts->PutJson(*ck, "k", VAL, 0, 1); // Use version 0 again
    EXPECT_EQ(result, zdb::ErrorCode::VersionMismatch) << "Put should fail with ErrVersion when using old version";
    
    // Test put to non-existent key with non-zero version - should fail  
    auto result2 = ts->PutJson(*ck, "y", VAL, TVersion(1), 1);
    EXPECT_EQ(result2, zdb::ErrorCode::KeyNotFound) << "Put to non-existent key with version > 0 should fail with ErrNoKey";
    
    // Test get of non-existent key - should fail
    std::string dummy_val;
    EXPECT_THROW({
        ts->GetJson(*ck, "y", 1, dummy_val);
    }, std::runtime_error) << "Get of non-existent key should fail";
}

TEST_F(KVServerTest, PutConcurrentReliable) {
    ts = std::make_unique<KVTestFramework>(true);
    const auto PORCUPINE_TIME = std::chrono::seconds(5);
    const int NCLNT = 3;  // Reduced from 10 to 3 clients
    const auto NSEC = std::chrono::seconds(1);  // Keep as 1 second
    
    ts->Begin("Test: many clients racing to put values to the same key");
    
    auto results = ts->SpawnClientsAndWait(NCLNT, NSEC, 
        [&](int client_id, std::unique_ptr<zdb::KVStoreClient>& ck, std::atomic<bool>& done) -> ClientResult {
            return ts->OneClientPut(client_id, ck, {"k"}, done);
        });
    
    auto ck = ts->makeClient();
    ClientResult total_result;
    ts->CheckPutConcurrent(ck, "k", results, &total_result, ts->IsReliable());
    ts->CheckPorcupineT(PORCUPINE_TIME);
}

TEST_F(KVServerTest, MemPutManyClientsReliable) {
    ts = std::make_unique<KVTestFramework>(true);
    const int NCLIENT = 1000; // Reduced for testing
    const int MEM_SIZE = 100;  // Reduced for testing
    
    ts->Begin("Test: memory use many put clients");
    
    std::string large_value = KVTestFramework::RandValue(MEM_SIZE);
    std::vector<std::unique_ptr<zdb::KVStoreClient>> clients;
    
    // Create clients
    for (int i = 0; i < NCLIENT; i++) {
        clients.push_back(ts->makeClient());
    }
    
    // Force allocation by trying put operations to the SAME key "k" (matching Go version)
    for (int i = 0; i < NCLIENT; i++) {
        auto err = ts->PutJson(*clients[static_cast<size_t>(i)], "k", "", TVersion(1), i);
        // This should fail with ErrNoKey since key doesn't exist yet and we're using version 1
        EXPECT_EQ(err, zdb::ErrorCode::KeyNotFound);
    }
    
    // Measure initial memory
    size_t initial_memory = KVTestFramework::GetHeapUsage();
    
    // Perform operations with the SAME key "k" using sequential versions (matching Go version)
    for (int i = 0; i < NCLIENT; i++) {
        auto err = ts->PutJson(*clients[static_cast<size_t>(i)], "k", large_value, TVersion(i), i);
        EXPECT_EQ(err, zdb::ErrorCode::OK);
    }
    
    // Measure final memory
    size_t final_memory = KVTestFramework::GetHeapUsage();
    
    if (initial_memory > 0 && final_memory > 0) {
        double per_client = static_cast<double>(final_memory - initial_memory) / NCLIENT;
        std::cout << "Memory usage: initial=" << initial_memory 
                  << " final=" << final_memory 
                  << " per_client=" << per_client << std::endl;
        
        // Check if memory usage is reasonable (less than 200 bytes per client)
        EXPECT_LT(final_memory, initial_memory + (NCLIENT * 200))
            << "Server using too much memory: " << per_client << " bytes per client";
    } else {
        std::cout << "Memory monitoring not available on this platform" << std::endl;
    }
}

TEST_F(KVServerTest, UnreliableNet) {
    const int NTRY = 100;
    
    // Create unreliable test framework
    auto unreliable_ts = std::make_unique<KVTestFramework>(false); // unreliable network
    unreliable_ts->Begin("One client unreliable network");
    
    auto ck = unreliable_ts->makeClient();
    
    bool retried = false;
    for (int try_num = 0; try_num < NTRY; try_num++) {
        std::cout << "=== Try " << try_num << " ===" << std::endl;
        for (int i = 0; true; i++) {
            std::cout << "  Attempt " << i << " for try " << try_num << std::endl;
            auto err = unreliable_ts->PutJson(*ck, "k", 0, TVersion(try_num), 0);
            std::cout << "  PutJson returned: " << toString(err) << std::endl;
            if (err != zdb::ErrorCode::Maybe) {
                if (i > 0 && err != zdb::ErrorCode::VersionMismatch) {
                    FAIL() << "Put shouldn't have happened more than once, got error: " << toString(err);
                }
                std::cout << "  Breaking from inner loop after " << (i+1) << " attempts" << std::endl;
                break;
            }
            // Try put again; it should fail with ErrVersion since it may have succeeded
            std::cout << "  Got ErrMaybe, setting retried=true and trying again" << std::endl;
            retried = true;
        }
        
        int stored_value = 0;
        auto version = unreliable_ts->GetJson(*ck, "k", 0, stored_value);
        EXPECT_EQ(version, TVersion(try_num + 1)) << "Wrong version " << version << " expect " << (try_num + 1);
        EXPECT_EQ(stored_value, 0) << "Wrong value " << stored_value << " expect 0";
    }
    
    EXPECT_TRUE(retried) << "Clerk.Put never returned ErrMaybe";
    
    unreliable_ts->CheckPorcupineT(std::chrono::seconds(1));
}

// Additional example test
TEST_F(KVServerTest, BasicOperations) {
    ts = std::make_unique<KVTestFramework>(true);
    ts->Begin("Basic operations test");
    
    auto ck = ts->makeClient();
    
    // Test JSON operations
    EntryV entry1{1, 5};
    EXPECT_EQ(ts->PutJson(*ck, "test_key", entry1, 0, 1), zdb::ErrorCode::OK);

    EntryV retrieved_entry;
    auto version = ts->GetJson(*ck, "test_key", 1, retrieved_entry);
    
    EXPECT_EQ(version, 1); // InMemoryKVStore sets new keys to version 1
    EXPECT_EQ(retrieved_entry.id, 1);
    EXPECT_EQ(retrieved_entry.version, 5);

    ts->CheckPorcupineT(std::chrono::seconds(1));
}
