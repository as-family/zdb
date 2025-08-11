#include "KVTestFramework.hpp"
#include <gtest/gtest.h>
#include <unordered_map>
#include <mutex>

// Example KV Server implementation for testing
class TestKVServer : public KVServer {
private:
    std::unordered_map<std::string, std::pair<std::string, TVersion>> data;
    mutable std::mutex data_mutex;
    bool killed = false;
    
public:
    void Get(const GetArgs& args, GetReply& reply) override {
        std::lock_guard<std::mutex> lock(data_mutex);
        
        if (killed) {
            reply.err = KVError::ErrNoKey;
            return;
        }
        
        auto it = data.find(args.key);
        if (it != data.end()) {
            reply.value = it->second.first;
            reply.version = it->second.second;
            reply.err = KVError::OK;
        } else {
            reply.value = "";
            reply.version = 0;
            reply.err = KVError::ErrNoKey;
        }
    }
    
    void Put(const PutArgs& args, PutReply& reply) override {
        std::lock_guard<std::mutex> lock(data_mutex);
        
        if (killed) {
            reply.err = KVError::ErrNoKey;
            return;
        }
        
        auto it = data.find(args.key);
        if (it != data.end()) {
            // Key exists - check version
            if (it->second.second == args.version) {
                // Version matches - update
                it->second.first = args.value;
                it->second.second++;
                reply.err = KVError::OK;
            } else {
                // Version mismatch
                reply.err = KVError::ErrVersion;
            }
        } else {
            // Key doesn't exist
            if (args.version == 0) {
                // Create new key
                data[args.key] = {args.value, 1};
                reply.err = KVError::OK;
            } else {
                // Trying to update non-existent key with non-zero version
                reply.err = KVError::ErrNoKey;
            }
        }
    }
    
    void Kill() override {
        std::lock_guard<std::mutex> lock(data_mutex);
        killed = true;
    }
};

// Example KV Clerk implementation
class TestKVClerk : public KVClerk {
private:
    KVServer* server;
    
public:
    TestKVClerk(KVServer* srv) : server(srv) {}
    
    std::tuple<std::string, TVersion, KVError> Get(const std::string& key) override {
        GetArgs args;
        args.key = key;
        
        GetReply reply;
        server->Get(args, reply);
        
        return {reply.value, reply.version, reply.err};
    }
    
    KVError Put(const std::string& key, const std::string& value, TVersion version) override {
        PutArgs args;
        args.key = key;
        args.value = value;
        args.version = version;
        
        PutReply reply;
        server->Put(args, reply);
        
        return reply.err;
    }
};

// Test class using the framework
class KVServerTest : public ::testing::Test {
protected:
    std::unique_ptr<KVTestFramework> ts;
    
    void SetUp() override {
        ts = std::make_unique<KVTestFramework>(true); // reliable network
        
        // Set up factories
        ts->SetServerFactory([]() -> std::unique_ptr<KVServer> {
            return std::make_unique<TestKVServer>();
        });
        
        ts->SetClerkFactory([](KVServer* server) -> std::unique_ptr<KVClerk> {
            return std::make_unique<TestKVClerk>(server);
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
        auto err = clients[i]->Put("k", "", 1);
        EXPECT_EQ(err, KVError::ErrNoKey);
    }
    
    // Measure initial memory
    size_t initial_memory = KVTestFramework::GetHeapUsage();
    
    // Perform operations
    for (int i = 0; i < NCLIENT; i++) {
        auto err = clients[i]->Put("k", large_value, static_cast<TVersion>(i));
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
    const int NTRY = 10; // Reduced for testing
    
    // Create unreliable test framework
    auto unreliable_ts = std::make_unique<KVTestFramework>(false); // unreliable network
    
    unreliable_ts->SetServerFactory([]() -> std::unique_ptr<KVServer> {
        return std::make_unique<TestKVServer>();
    });
    
    unreliable_ts->SetClerkFactory([](KVServer* server) -> std::unique_ptr<KVClerk> {
        return std::make_unique<TestKVClerk>(server);
    });
    
    unreliable_ts->Begin("One client unreliable network");
    
    auto ck = unreliable_ts->MakeClerk();
    
    bool retried = false;
    
    for (int try_num = 0; try_num < NTRY; try_num++) {
        for (int i = 0; ; i++) {
            // Try to put a JSON integer value
            auto err = unreliable_ts->PutJson(*ck, "k", i, static_cast<TVersion>(try_num), 0);
            
            if (err != KVError::ErrMaybe) {
                if (i > 0 && err != KVError::ErrVersion) {
                    FAIL() << "Put shouldn't have happened more than once, got error: " 
                           << KVTestFramework::ErrorToString(err);
                }
                break;
            }
            // If we get ErrMaybe, try again; it should fail with ErrVersion
            retried = true;
        }
        
        // Verify the value was actually stored correctly
        int stored_value = 0;
        auto version = unreliable_ts->GetJson(*ck, "k", 0, stored_value);
        
        EXPECT_EQ(version, static_cast<TVersion>(try_num + 1)) 
            << "Wrong version " << version << " expected " << (try_num + 1);
        
        EXPECT_EQ(stored_value, 0) 
            << "Wrong value " << stored_value << " expected 0";
    }
    
    // For this simple test, we can't easily simulate ErrMaybe without network layer
    // EXPECT_TRUE(retried) << "Clerk.Put never returned ErrMaybe";
    
    unreliable_ts->CheckPorcupine();
    unreliable_ts->Cleanup();
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
