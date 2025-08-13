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

#include "client/KVStoreClient.hpp"
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include "common/Types.hpp"
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include "client/Config.hpp"

class NetworkSimulator;
class PorcupineChecker;

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
    zdb::ErrorCode err;
};

struct GetArgs {
    std::string key;
};

struct GetReply {
    std::string value;
    TVersion version;
    zdb::ErrorCode err;
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
    // Call external Go porcupine checker
    bool CallPorcupineChecker(const std::string& json_file);
    
    // Convert operations to JSON format for Go checker
    nlohmann::json OperationsToJson() const;
};


// gRPC Proxy for network simulation
class KVStoreProxyService final : public zdb::kvStore::KVStoreService::Service {
public:
    KVStoreProxyService(const std::string& real_server_addr, std::unique_ptr<NetworkSimulator> net_sim)
        : real_server_addr_(real_server_addr), network_sim_(std::move(net_sim)) {
        channel_ = grpc::CreateChannel(real_server_addr_, grpc::InsecureChannelCredentials());
        stub_ = zdb::kvStore::KVStoreService::NewStub(channel_);
    }

    grpc::Status get(grpc::ServerContext* context, const zdb::kvStore::GetRequest* request, zdb::kvStore::GetReply* reply) override {
        if (network_sim_ && network_sim_->ShouldDropMessage()) {
            return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Simulated drop");
        }
        if (network_sim_ && network_sim_->ShouldDelayMessage()) {
            std::this_thread::sleep_for(network_sim_->GetRandomDelay());
        }
        grpc::ClientContext client_ctx;
        return stub_->get(&client_ctx, *request, reply);
    }

    grpc::Status set(grpc::ServerContext* context, const zdb::kvStore::SetRequest* request, zdb::kvStore::SetReply* reply) override {
        if (network_sim_ && network_sim_->ShouldDropMessage()) {
            return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Simulated drop");
        }
        if (network_sim_ && network_sim_->ShouldDelayMessage()) {
            std::this_thread::sleep_for(network_sim_->GetRandomDelay());
        }
        grpc::ClientContext client_ctx;
        return stub_->set(&client_ctx, *request, reply);
    }

    grpc::Status erase(grpc::ServerContext* context, const zdb::kvStore::EraseRequest* request, zdb::kvStore::EraseReply* reply) override {
        if (network_sim_ && network_sim_->ShouldDropMessage()) {
            return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Simulated drop");
        }
        if (network_sim_ && network_sim_->ShouldDelayMessage()) {
            std::this_thread::sleep_for(network_sim_->GetRandomDelay());
        }
        grpc::ClientContext client_ctx;
        return stub_->erase(&client_ctx, *request, reply);
    }

    grpc::Status size(grpc::ServerContext* context, const zdb::kvStore::SizeRequest* request, zdb::kvStore::SizeReply* reply) override {
        if (network_sim_ && network_sim_->ShouldDropMessage()) {
            return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Simulated drop");
        }
        if (network_sim_ && network_sim_->ShouldDelayMessage()) {
            std::this_thread::sleep_for(network_sim_->GetRandomDelay());
        }
        grpc::ClientContext client_ctx;
        return stub_->size(&client_ctx, *request, reply);
    }

private:
    std::string real_server_addr_;
    std::unique_ptr<NetworkSimulator> network_sim_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub_;
};

class KVStoreServerProxy {
public:
    KVStoreServerProxy(const std::string& address, std::unique_ptr<KVStoreProxyService> s);

    void wait();
    void shutdown();

private:
    std::string addr;
    std::unique_ptr<KVStoreProxyService> service;
    std::unique_ptr<grpc::Server> server;
};

// Main test framework
class KVTestFramework {
private:
    bool reliable_network;
    std::unique_ptr<NetworkSimulator> network_sim;
    std::unique_ptr<zdb::InMemoryKVStore> kvStore;
    std::unique_ptr<zdb::KVStoreServiceImpl> realServiceImpl;
    std::unique_ptr<zdb::KVStoreServer> realServer;
    std::thread realServerThread;
    std::unique_ptr<KVStoreProxyService> proxyService;
    std::unique_ptr<KVStoreServerProxy> proxyServer;
    std::thread proxyServerThread;
    std::unique_ptr<PorcupineChecker> porcupine_checker;
    std::string current_test_name;
    std::string real_server_address;
    std::string proxy_server_address;
    std::unique_ptr<zdb::Config> config; // Store config for clients
    
public:
    KVTestFramework(bool reliable = true);
    ~KVTestFramework();
    
    // Core test utilities
    std::unique_ptr<zdb::KVStoreClient> makeClient();
    void Begin(const std::string& test_name);
    
    // JSON helpers (equivalent to Go's PutJson/GetJson)
    template<typename T>
    zdb::ErrorCode PutJson(zdb::KVStoreClient& ck, const std::string& key, const T& value, 
                    TVersion version, int client_id = 0);
    
    template<typename T>
    TVersion GetJson(zdb::KVStoreClient& ck, const std::string& key, int client_id, T& result);
    
    // Porcupine testing
    void CheckPorcupineT(std::chrono::seconds timeout);
    
    // Concurrency testing
    std::vector<ClientResult> SpawnClientsAndWait(
        size_t num_clients, 
        std::chrono::seconds duration,
        std::function<ClientResult(int, std::unique_ptr<zdb::KVStoreClient>&, std::atomic<bool>&)> client_fn);
    
    // Additional overload for compatibility
    void CheckPutConcurrent(std::unique_ptr<zdb::KVStoreClient>& ck, const std::string& key, 
                           const std::vector<ClientResult>& results, ClientResult* total_result, bool reliable);
    
    // Utility functions
    ClientResult OneClientPut(int client_id, std::unique_ptr<zdb::KVStoreClient>& ck, 
                             const std::vector<std::string>& keys, 
                             std::atomic<bool>& done);
    
    std::pair<TVersion, bool> OnePut(int client_id, zdb::KVStoreClient& ck, 
                                    const std::string& key, TVersion version,
                                    std::atomic<bool>& done);
    
    bool IsReliable() const { return reliable_network; }
    
    // Memory utilities
    static size_t GetHeapUsage();
    
    // Utility functions
    static std::string RandValue(int length);

private:
    void RunClient(int client_id, 
                  std::function<ClientResult(int, std::unique_ptr<zdb::KVStoreClient>&, std::atomic<bool>&)> client_fn,
                  std::atomic<bool>& done,
                  std::promise<ClientResult>& result_promise);
};

// Template implementations
template<typename T>
zdb::ErrorCode KVTestFramework::PutJson(zdb::KVStoreClient& ck, const std::string& key, 
                                const T& value, TVersion version, int client_id) {
    nlohmann::json j = value;
    std::string json_str = j.dump();

    auto start_time = std::chrono::steady_clock::now();

    zdb::ErrorCode err;
    zdb::Key zdbKey{key};
    zdb::Value zdbValue{json_str, version};
    auto result = ck.set(zdbKey, zdbValue);
    err = errorCode(result);
    
    auto end_time = std::chrono::steady_clock::now();
    
    if (porcupine_checker) {
        PorcupineOperation op;
        op.input = {OpType::PUT, key, json_str, version};
        op.output = {"", 0, toString(err)};
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
TVersion KVTestFramework::GetJson(zdb::KVStoreClient& ck, const std::string& key, 
                                 int client_id, T& result) {
    auto start_time = std::chrono::steady_clock::now();
    
    zdb::Key zdbKey{key};
    auto getResult = ck.get(zdbKey);
    
    auto end_time = std::chrono::steady_clock::now();
    
    if (getResult.has_value()) {
        nlohmann::json j = nlohmann::json::parse(getResult.value().data);
        result = j.get<T>();
        
        if (porcupine_checker) {
            PorcupineOperation op;
            op.input = {OpType::GET, key, "", 0};
            op.output = {getResult.value().data, getResult.value().version, toString(zdb::ErrorCode::OK)};
            op.call_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                start_time.time_since_epoch()).count();
            op.return_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time.time_since_epoch()).count();
            op.client_id = client_id;
            porcupine_checker->LogOperation(op);
        }
        
        return getResult.value().version;
    } else {
        if (porcupine_checker) {
            PorcupineOperation op;
            op.input = {OpType::GET, key, "", 0};
            op.output = {"", 0, toString(zdb::ErrorCode::KeyNotFound)};
            op.call_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                start_time.time_since_epoch()).count();
            op.return_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                end_time.time_since_epoch()).count();
            op.client_id = client_id;
            porcupine_checker->LogOperation(op);
        }
        throw std::runtime_error("Get failed for key '" + key + "': " + toString(zdb::ErrorCode::KeyNotFound));
    }
}
