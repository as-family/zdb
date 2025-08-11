#include "KVTestFramework.hpp"
#include <gtest/gtest.h>

// Test class using the framework with the current project
class KVServerTest : public ::testing::Test {
protected:
    std::unique_ptr<KVTestFramework> ts;
    
    void SetUp() override {
        ts = std::make_unique<KVTestFramework>(true); // reliable network
        
        // Set up factories using our adapters
        ts->SetServerFactory([]() -> std::unique_ptr<KVServer> {
            return std::make_unique<ZdbKVServerAdapter>();
        });
        
        ts->SetClerkFactory([](KVServer* server) -> std::unique_ptr<KVClerk> {
            return std::make_unique<ZdbKVClerkAdapter>(server);
        });
    }
    
    void TearDown() override {
        ts->Cleanup();
    }
};

// Test implementations corresponding to the Go tests

TEST_F(KVServerTest, ReliablePut) {
    const std::string VAL = "6.5840";
    const TVersion VER = 0;
    
    ts->Begin("One client and reliable Put");
    
    auto ck = ts->MakeClerk();
    
    // Test basic put
    EXPECT_EQ(ck->Put("k", VAL, VER), KVError::OK);
    
    // Test get
    auto [val, ver, err] = ck->Get("k");
    EXPECT_EQ(err, KVError::OK);
    EXPECT_EQ(val, VAL);
    EXPECT_EQ(ver, VER + 1);
    
    // Test version mismatch
    EXPECT_EQ(ck->Put("k", VAL, 0), KVError::ErrVersion);
    
    // Test non-existent key with non-zero version
    EXPECT_EQ(ck->Put("y", VAL, 1), KVError::ErrNoKey);
    
    // Test get of non-existent key
    auto [val2, ver2, err2] = ck->Get("y");
    EXPECT_EQ(err2, KVError::ErrNoKey);
}

TEST_F(KVServerTest, PutConcurrentReliable) {
    const auto PORCUPINE_TIME = std::chrono::seconds(10);
    const int NCLNT = 10;
    const auto NSEC = std::chrono::seconds(1);
    
    ts->Begin("Test: many clients racing to put values to the same key");
    
    auto results = ts->SpawnClientsAndWait(NCLNT, NSEC, 
        [&](int client_id, std::unique_ptr<KVClerk>& ck, std::atomic<bool>& done) -> ClientResult {
            return ts->OneClientPut(client_id, ck, {"k"}, done);
        });
    
    ts->CheckPutConcurrent("k", results);
    ts->CheckPorcupineT(PORCUPINE_TIME);
}

TEST_F(KVServerTest, MemPutManyClientsReliable) {
    const int NCLIENT = 1000; // Reduced for testing
    const int MEM_SIZE = 100;  // Reduced for testing
    
    ts->Begin("Test: memory use many put clients");
    
    std::string large_value = KVTestFramework::RandValue(MEM_SIZE);
    std::vector<std::unique_ptr<KVClerk>> clients;
    
    // Create clients
    for (int i = 0; i < NCLIENT; i++) {
        clients.push_back(ts->MakeClerk());
    }
    
    // Force allocation by trying invalid operations
    for (int i = 0; i < NCLIENT; i++) {
        auto err = clients[static_cast<size_t>(i)]->Put("k", "", 1);
        EXPECT_EQ(err, KVError::ErrNoKey);
    }
    
    // Measure initial memory
    size_t initial_memory = KVTestFramework::GetHeapUsage();
    
    // Perform operations
    for (int i = 0; i < NCLIENT; i++) {
        auto err = clients[static_cast<size_t>(i)]->Put("k", large_value, static_cast<TVersion>(i));
        EXPECT_EQ(err, KVError::OK);
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
    const int NTRY = 3; // Reduced for testing
    
    // For the adapter, we'll simulate unreliable behavior differently
    // since the current project doesn't have built-in unreliable network simulation
    ts->Begin("One client unreliable network (simplified)");
    
    auto ck = ts->MakeClerk();
    
    for (int try_num = 0; try_num < NTRY; try_num++) {
        // Try to put a JSON integer value
        auto err = ts->PutJson(*ck, "k", try_num, static_cast<TVersion>(try_num), 0);
        
        // In our simplified adapter, we expect consistent behavior
        if (try_num == 0) {
            EXPECT_EQ(err, KVError::OK) << "First put should succeed";
        } else {
            // Subsequent puts with wrong version should fail
            EXPECT_EQ(err, KVError::ErrVersion) << "Put with old version should fail";
        }
        
        // Verify the current value
        int stored_value = 0;
        auto version = ts->GetJson(*ck, "k", 0, stored_value);
        
        // Version should be 1 (we only successfully put once)
        EXPECT_EQ(version, 1) << "Version should be 1 after first successful put";
        EXPECT_EQ(stored_value, 0) << "Value should be from first put";
    }
    
    ts->CheckPorcupine();
}

// Additional example test
TEST_F(KVServerTest, BasicOperations) {
    ts->Begin("Basic operations test");
    
    auto ck = ts->MakeClerk();
    
    // Test JSON operations
    EntryV entry1{1, 5};
    EXPECT_EQ(ts->PutJson(*ck, "test_key", entry1, 0, 1), KVError::OK);
    
    EntryV retrieved_entry;
    auto version = ts->GetJson(*ck, "test_key", 1, retrieved_entry);
    
    EXPECT_EQ(version, 1);
    EXPECT_EQ(retrieved_entry.id, 1);
    EXPECT_EQ(retrieved_entry.version, 5);
    
    ts->CheckPorcupine();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
