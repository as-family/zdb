#pragma once

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <string>
#include <mutex>
#include <random>
#include <fstream>
#include <sstream>
#include <future>

// Include current project headers
#include "client/KVStoreClient.hpp"
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include "common/Types.hpp"
#include "common/Error.hpp"
#include "client/Config.hpp"

// Forward declarations
class KVServer;
class KVClerk;
class NetworkSimulator;
class PorcupineChecker;

// Error types matching the current project
enum class KVError {
    OK,
    ErrNoKey,
    ErrVersion,
    ErrMaybe  // Client-side only
};

// Helper function to convert from current project errors
KVError ErrorFromZdb(const zdb::Error& err);
KVError ErrorFromZdb(const std::expected<zdb::Value, zdb::Error>& result);
KVError ErrorFromZdb(const std::expected<void, zdb::Error>& result);

// Version type
using TVersion = uint64_t;

// Operation types for porcupine
enum class OpType : uint8_t {
    GET = 0,
    PUT = 1
};

// RPC structures
struct PutArgs {
    std::string key;
    std::string value;
    TVersion version;
};

struct PutReply {
    KVError err;
};

struct GetArgs {
    std::string key;
};

struct GetReply {
    std::string value;
    TVersion version;
    KVError err;
};

// Porcupine operation tracking
struct KVInput {
    OpType op;
    std::string key;
    std::string value;
    uint64_t version;
};

struct KVOutput {
    std::string value;
    uint64_t version;
    std::string err;
};

struct KVState {
    std::string value;
    uint64_t version;
    
    KVState() : value(""), version(0) {}
    KVState(const std::string& v, uint64_t ver) : value(v), version(ver) {}
};

struct PorcupineOperation {
    KVInput input;
    KVOutput output;
    int64_t call_time;
    int64_t return_time;
    int client_id;
};

// Client result tracking for concurrent tests
struct ClientResult {
    int nok = 0;     // Number of successful operations
    int nmaybe = 0;  // Number of ErrMaybe responses
    
    ClientResult& operator+=(const ClientResult& other) {
        nok += other.nok;
        nmaybe += other.nmaybe;
        return *this;
    }
};

// Entry structure for JSON operations (equivalent to Go's EntryV)
struct EntryV {
    int id;
    TVersion version;
    
    // JSON serialization support
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(EntryV, id, version)
};

// Network simulator for unreliable testing
class NetworkSimulator {
private:
    bool reliable;
    mutable std::random_device rd;
    mutable std::mt19937 gen;
    mutable std::uniform_real_distribution<> dis;
    
    // Configurable failure rates
    double drop_rate = 0.1;       // 10% message drop rate
    double delay_rate = 0.15;     // 15% message delay rate
    double duplicate_rate = 0.05; // 5% message duplication rate
    
public:
    NetworkSimulator(bool isReliable = true);
    
    bool ShouldDropMessage() const;
    bool ShouldDelayMessage() const;
    bool ShouldDuplicateMessage() const;
    std::chrono::milliseconds GetRandomDelay() const;
    
    void SetReliable(bool isReliable) { this->reliable = isReliable; }
    bool IsReliable() const { return reliable; }
};

// Porcupine linearizability checker
class PorcupineChecker {
private:
    std::vector<PorcupineOperation> operations;
    mutable std::mutex ops_mutex;
    std::chrono::steady_clock::time_point start_time;
    
public:
    PorcupineChecker();
    
    void LogOperation(const PorcupineOperation& op);
    bool CheckLinearizability(std::chrono::seconds timeout);
    void Clear();
    size_t GetOperationCount() const;
    
private:
    bool CheckPartitionLinearizability(const std::vector<PorcupineOperation>& ops, 
                                     std::chrono::seconds timeout);
    std::pair<bool, KVState> StepFunction(const KVState& state, 
                                         const KVInput& input, 
                                         const KVOutput& output);
    std::map<std::string, std::vector<PorcupineOperation>> PartitionByKey() const;
};

// Simple KV Server interface (adapter for current project)
class KVServer {
public:
    virtual ~KVServer() = default;
    virtual void Get(const GetArgs& args, GetReply& reply) = 0;
    virtual void Put(const PutArgs& args, PutReply& reply) = 0;
    virtual void Kill() = 0;
};

// Adapter for current project's server (simplified for direct access)
class ZdbKVServerAdapter : public KVServer {
private:
    std::unique_ptr<zdb::InMemoryKVStore> kvStore;
    bool killed = false;
    mutable std::mutex store_mutex; // Add thread safety
    
public:
    ZdbKVServerAdapter();
    ~ZdbKVServerAdapter();
    void Get(const GetArgs& args, GetReply& reply) override;
    void Put(const PutArgs& args, PutReply& reply) override;
    void Kill() override;
    zdb::InMemoryKVStore* GetKVStore() { return kvStore.get(); }
};

// Simple KV Clerk interface (adapter for current project)
class KVClerk {
public:
    virtual ~KVClerk() = default;
    virtual std::tuple<std::string, TVersion, KVError> Get(const std::string& key) = 0;
    virtual KVError Put(const std::string& key, const std::string& value, TVersion version) = 0;
};

// Adapter for current project's client
class ZdbKVClerkAdapter : public KVClerk {
private:
    KVServer* server; // Use server interface directly for thread safety
    
public:
    ZdbKVClerkAdapter(KVServer* srv);
    std::tuple<std::string, TVersion, KVError> Get(const std::string& key) override;
    KVError Put(const std::string& key, const std::string& value, TVersion version) override;
};

// Main test framework
class KVTestFramework {
private:
    bool reliable_network;
    std::unique_ptr<NetworkSimulator> network_sim;
    std::unique_ptr<KVServer> server;
    std::unique_ptr<PorcupineChecker> porcupine_checker;
    std::string current_test_name;
    bool cleanup_done;
    
    // Function to create server (injected by user)
    std::function<std::unique_ptr<KVServer>()> server_factory;
    
    // Function to create clerk (injected by user)
    std::function<std::unique_ptr<KVClerk>(KVServer*)> clerk_factory;
    
public:
    KVTestFramework(bool reliable = true);
    ~KVTestFramework();
    
    // Configuration
    void SetServerFactory(std::function<std::unique_ptr<KVServer>()> factory);
    void SetClerkFactory(std::function<std::unique_ptr<KVClerk>(KVServer*)> factory);
    
    // Core test utilities
    std::unique_ptr<KVClerk> MakeClerk();
    void Begin(const std::string& test_name);
    void Cleanup();
    
    // JSON helpers (equivalent to Go's PutJson/GetJson)
    template<typename T>
    KVError PutJson(KVClerk& ck, const std::string& key, const T& value, 
                    TVersion version, int client_id = 0);
    
    template<typename T>
    TVersion GetJson(KVClerk& ck, const std::string& key, int client_id, T& result);
    
    // Porcupine testing
    void CheckPorcupine();
    void CheckPorcupineT(std::chrono::seconds timeout);
    
    // Concurrency testing
    std::vector<ClientResult> SpawnClientsAndWait(
        int num_clients, 
        std::chrono::seconds duration,
        std::function<ClientResult(int, std::unique_ptr<KVClerk>&, std::atomic<bool>&)> client_fn);
    
    void CheckPutConcurrent(const std::string& key, 
                           const std::vector<ClientResult>& results);
    
    // Utility functions
    ClientResult OneClientPut(int client_id, std::unique_ptr<KVClerk>& ck, 
                             const std::vector<std::string>& keys, 
                             std::atomic<bool>& done);
    
    std::pair<TVersion, bool> OnePut(int client_id, KVClerk& ck, 
                                    const std::string& key, TVersion version);
    
    bool IsReliable() const { return reliable_network; }
    
    // Memory utilities
    static size_t GetHeapUsage();
    
    // Utility functions
    static std::string ErrorToString(KVError err);
    static std::string RandValue(int length);

private:
    void InitializeServer();
    void RunClient(int client_id, 
                  std::function<ClientResult(int, std::unique_ptr<KVClerk>&, std::atomic<bool>&)> client_fn,
                  std::atomic<bool>& done,
                  std::promise<ClientResult>& result_promise);
};

// Template implementations
template<typename T>
KVError KVTestFramework::PutJson(KVClerk& ck, const std::string& key, 
                                const T& value, TVersion version, int client_id) {
    nlohmann::json j = value;
    std::string json_str = j.dump();
    
    // Log for porcupine if enabled
    auto start_time = std::chrono::steady_clock::now();
    auto err = ck.Put(key, json_str, version);
    auto end_time = std::chrono::steady_clock::now();
    
    if (porcupine_checker) {
        PorcupineOperation op;
        op.input = {OpType::PUT, key, json_str, version};
        op.output = {"", 0, ErrorToString(err)};
        op.call_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            start_time.time_since_epoch()).count();
        op.return_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time.time_since_epoch()).count();
        op.client_id = client_id;
        porcupine_checker->LogOperation(op);
    }
    
    return err;
}

template<typename T>
TVersion KVTestFramework::GetJson(KVClerk& ck, const std::string& key, 
                                 int client_id, T& result) {
    auto start_time = std::chrono::steady_clock::now();
    auto [value, version, err] = ck.Get(key);
    auto end_time = std::chrono::steady_clock::now();
    
    if (err == KVError::OK) {
        nlohmann::json j = nlohmann::json::parse(value);
        result = j.get<T>();
        
        if (porcupine_checker) {
            PorcupineOperation op;
            op.input = {OpType::GET, key, "", 0};
            op.output = {value, version, ErrorToString(err)};
            op.call_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                start_time.time_since_epoch()).count();
            op.return_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time.time_since_epoch()).count();
            op.client_id = client_id;
            porcupine_checker->LogOperation(op);
        }
        
        return version;
    } else {
        throw std::runtime_error("Get failed for key '" + key + "': " + ErrorToString(err));
    }
}
