#include "KVTestFramework.hpp"
#include <iostream>
#include <future>
#include <algorithm>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include "common/RetryPolicy.hpp"

#ifdef __linux__
#include <fstream>
#include <unistd.h>
#include <sys/resource.h>
#endif

// Helper function implementations
KVError ErrorFromZdb(const zdb::Error& err) {
    switch (err.code) {
        case zdb::ErrorCode::NotFound:
            return KVError::ErrNoKey;
        case zdb::ErrorCode::InvalidArg:
            return KVError::ErrVersion;
        case zdb::ErrorCode::VersionMismatch:
            return KVError::ErrVersion;
        default:
            return KVError::ErrMaybe;
    }
}

KVError ErrorFromZdb(const std::expected<zdb::Value, zdb::Error>& result) {
    if (result.has_value()) {
        return KVError::OK;
    }
    return ErrorFromZdb(result.error());
}

KVError ErrorFromZdb(const std::expected<void, zdb::Error>& result) {
    if (result.has_value()) {
        return KVError::OK;
    }
    return ErrorFromZdb(result.error());
}

// NetworkSimulator Implementation
NetworkSimulator::NetworkSimulator(bool isReliable) 
    : reliable(isReliable), gen(rd()), dis(0.0, 1.0) {}

bool NetworkSimulator::ShouldDropMessage() const {
    return !reliable && dis(gen) < drop_rate;
}

bool NetworkSimulator::ShouldDelayMessage() const {
    return !reliable && dis(gen) < delay_rate;
}

bool NetworkSimulator::ShouldDuplicateMessage() const {
    return !reliable && dis(gen) < duplicate_rate;
}

std::chrono::milliseconds NetworkSimulator::GetRandomDelay() const {
    if (!reliable) {
        return std::chrono::milliseconds(
            static_cast<int>(dis(gen) * 100 + 10)); // 10-110ms delay
    }
    return std::chrono::milliseconds(0);
}

// PorcupineChecker Implementation
PorcupineChecker::PorcupineChecker() 
    : start_time(std::chrono::steady_clock::now()) {}

void PorcupineChecker::LogOperation(const PorcupineOperation& op) {
    std::lock_guard<std::mutex> lock(ops_mutex);
    operations.push_back(op);
}

void PorcupineChecker::Clear() {
    std::lock_guard<std::mutex> lock(ops_mutex);
    operations.clear();
    start_time = std::chrono::steady_clock::now();
}

size_t PorcupineChecker::GetOperationCount() const {
    std::lock_guard<std::mutex> lock(ops_mutex);
    return operations.size();
}

nlohmann::json PorcupineChecker::OperationsToJson() const {
    nlohmann::json j = nlohmann::json::array();
    
    for (const auto& op : operations) {
        nlohmann::json op_json;
        op_json["client_id"] = op.client_id;
        op_json["call_time"] = op.call_time;
        op_json["return_time"] = op.return_time;
        
        op_json["input"]["op"] = static_cast<int>(op.input.op);
        op_json["input"]["key"] = op.input.key;
        op_json["input"]["value"] = op.input.value;
        op_json["input"]["version"] = op.input.version;
        
        op_json["output"]["value"] = op.output.value;
        op_json["output"]["version"] = op.output.version;
        op_json["output"]["error"] = op.output.err;
        
        j.push_back(op_json);
    }
    
    return j;
}

bool PorcupineChecker::CallPorcupineChecker(const std::string& json_file) {
    // Call the Go porcupine checker
    std::string command = "/home/ahmed/ws/zdb/tst/kvTest1/porcupine_checker " + json_file;
    int result = system(command.c_str());
    return result == 0;
}

bool PorcupineChecker::CheckLinearizability(std::chrono::seconds /* timeout */) {
    std::lock_guard<std::mutex> lock(ops_mutex);
    
    if (operations.empty()) {
        return true;
    }
    
    // Convert operations to JSON
    auto json_ops = OperationsToJson();
    
    // Write to temporary file
    std::string temp_file = "/tmp/porcupine_ops_" + std::to_string(getpid()) + ".json";
    std::ofstream file(temp_file);
    if (!file.is_open()) {
        std::cerr << "Failed to create temporary file: " << temp_file << std::endl;
        return false;
    }
    
    file << json_ops.dump(2);
    file.close();
    
    // Call Go porcupine checker
    bool result = CallPorcupineChecker(temp_file);
    
    // Clean up temporary file
    std::remove(temp_file.c_str());
    
    return result;
}

// KVTestFramework Implementation
KVTestFramework::KVTestFramework(bool reliable) 
    : reliable_network(reliable), cleanup_done(false), server_address("localhost:50051") {
    
    network_sim = std::make_unique<NetworkSimulator>(reliable);
    porcupine_checker = std::make_unique<PorcupineChecker>();
}

KVTestFramework::~KVTestFramework() {
    if (!cleanup_done) {
        Cleanup();
    }
}

void KVTestFramework::InitializeServer() {
    std::cout << "Initializing server on " << server_address << std::endl;
    
    try {
        kvStore = std::make_unique<zdb::InMemoryKVStore>();
        serviceImpl = std::make_unique<zdb::KVStoreServiceImpl>(*kvStore);
        server = std::make_unique<zdb::KVStoreServer>(server_address, *serviceImpl);
        
        std::cout << "Server created, starting thread..." << std::endl;
        serverThread = std::thread([this]() { 
            std::cout << "Server thread started, calling wait()..." << std::endl;
            server->wait(); 
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow server to start
        std::cout << "Server initialization complete" << std::endl;
        
        // Create config for clients
        zdb::RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
        std::vector<std::string> addresses{server_address};
        config = std::make_unique<zdb::Config>(addresses, policy);
    } catch (const std::exception& e) {
        std::cerr << "Server initialization failed: " << e.what() << std::endl;
        throw;
    }
}

std::unique_ptr<zdb::KVStoreClient> KVTestFramework::MakeClerk() {
    if (!server) {
        InitializeServer();
    }
    
    return std::make_unique<zdb::KVStoreClient>(*config);
}

void KVTestFramework::Begin(const std::string& test_name) {
    current_test_name = test_name;
    std::cout << "Starting test: " << test_name << std::endl;
    
    // Initialize server if not already done
    if (!server) {
        InitializeServer();
    }
    
    // Clear any previous porcupine operations
    if (porcupine_checker) {
        porcupine_checker->Clear();
    }
}

void KVTestFramework::Cleanup() {
    if (cleanup_done) return;
    
    if (server) {
        server->shutdown();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        server.reset();
    }
    
    cleanup_done = true;
    
    if (!current_test_name.empty()) {
        std::cout << "Completed test: " << current_test_name << std::endl;
    }
}

void KVTestFramework::CheckPorcupine() {
    CheckPorcupineT(std::chrono::seconds(1));
}

void KVTestFramework::CheckPorcupineT(std::chrono::seconds timeout) {
    if (!porcupine_checker) {
        std::cerr << "Warning: Porcupine checker not available" << std::endl;
        return;
    }
    
    std::cout << "Checking linearizability with " 
              << porcupine_checker->GetOperationCount() 
              << " operations..." << std::endl;
    
    bool is_linearizable = porcupine_checker->CheckLinearizability(timeout);
    
    if (!is_linearizable) {
        throw std::runtime_error("History is not linearizable");
    }
    
    std::cout << "Linearizability check passed." << std::endl;
}

std::vector<ClientResult> KVTestFramework::SpawnClientsAndWait(
    int num_clients, 
    std::chrono::seconds duration,
    std::function<ClientResult(int, std::unique_ptr<zdb::KVStoreClient>&, std::atomic<bool>&)> client_fn) {
    
    // Initialize server before starting any client threads to avoid race condition
    if (!server) {
        InitializeServer();
    }
    
    std::vector<std::thread> threads;
    std::vector<std::promise<ClientResult>> promises(static_cast<size_t>(num_clients));
    std::vector<std::future<ClientResult>> futures;
    std::atomic<bool> done{false};
    
    // Get futures from promises
    for (auto& promise : promises) {
        futures.push_back(promise.get_future());
    }
    
    // Start all client threads
    for (int i = 0; i < num_clients; i++) {
        threads.emplace_back(&KVTestFramework::RunClient, this, i, client_fn, 
                           std::ref(done), std::ref(promises[static_cast<size_t>(i)]));
    }
    
    // Wait for specified duration
    std::this_thread::sleep_for(duration);
    
    // Signal all clients to stop
    done = true;
    
    // Collect results
    std::vector<ClientResult> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    return results;
}

void KVTestFramework::RunClient(int client_id, 
                               std::function<ClientResult(int, std::unique_ptr<zdb::KVStoreClient>&, std::atomic<bool>&)> client_fn,
                               std::atomic<bool>& done,
                               std::promise<ClientResult>& result_promise) {
    try {
        auto ck = MakeClerk();
        ClientResult result = client_fn(client_id, ck, done);
        result_promise.set_value(result);
    } catch (const std::exception& e) {
        std::cerr << "Client " << client_id << " failed: " << e.what() << std::endl;
        result_promise.set_exception(std::current_exception());
    }
}

void KVTestFramework::CheckPutConcurrent(const std::string& key, 
                                        const std::vector<ClientResult>& results) {
    ClientResult total_result;
    for (const auto& result : results) {
        total_result += result;
    }
    
    // Get current state of the key
    auto ck = MakeClerk();
    EntryV entry;
    try {
        TVersion current_version = GetJson(*ck, key, -1, entry);
        
        if (IsReliable()) {
            if (current_version != static_cast<TVersion>(total_result.nok)) {
                throw std::runtime_error(
                    "Reliable: Wrong number of puts: server version " + 
                    std::to_string(current_version) + 
                    " but clients succeeded " + std::to_string(total_result.nok) + 
                    " times (maybe " + std::to_string(total_result.nmaybe) + ")");
            }
        } else {
            if (current_version > static_cast<TVersion>(total_result.nok + total_result.nmaybe)) {
                throw std::runtime_error(
                    "Unreliable: Wrong number of puts: server version " + 
                    std::to_string(current_version) + 
                    " but clients succeeded at most " + 
                    std::to_string(total_result.nok + total_result.nmaybe) + " times");
            }
        }
        
        std::cout << "Concurrent put check passed: version=" << current_version 
                  << " nok=" << total_result.nok 
                  << " nmaybe=" << total_result.nmaybe << std::endl;
        
    } catch (const std::runtime_error& e) {
        // Key might not exist if no operations succeeded
        if (total_result.nok == 0) {
            std::cout << "No successful puts, key doesn't exist (expected)" << std::endl;
        } else {
            throw;
        }
    }
}

ClientResult KVTestFramework::OneClientPut(int client_id, std::unique_ptr<zdb::KVStoreClient>& ck, 
                                          const std::vector<std::string>& keys, 
                                          std::atomic<bool>& done) {
    ClientResult result;
    std::map<std::string, TVersion> version_map;
    
    // Initialize versions to 0 for all keys
    for (const auto& key : keys) {
        version_map[key] = 0;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> key_dist(0, keys.size() - 1);
    
    while (!done.load()) {
        // Select a random key (or just use first key for simple case)
        std::string key = keys[key_dist(gen)];
        
        auto [new_version, success] = OnePut(client_id, *ck, key, version_map[key]);
        
        if (success) {
            result.nok++;
            version_map[key] = new_version;
        } else {
            result.nmaybe++;
            version_map[key] = new_version;
        }
    }
    
    return result;
}

std::pair<TVersion, bool> KVTestFramework::OnePut(int client_id, zdb::KVStoreClient& ck, 
                                                 const std::string& key, TVersion version) {
    while (true) {
        // Step 1: Get current version of the key
        TVersion current_version = 0;
        EntryV current_entry;
        bool key_exists = false;
        
        try {
            current_version = GetJson(ck, key, client_id, current_entry);
            key_exists = true;
        } catch (const std::runtime_error& e) {
            // Key doesn't exist yet, start with version 0
            key_exists = false;
            current_version = 0;
        }
        
        // Step 2: Create new entry with incremented version
        TVersion new_version = current_version + 1;
        EntryV entry{client_id, version}; // version here is the logical version from test
        
        // Step 3: Try to put with the new version
        auto err = PutJson(ck, key, entry, new_version, client_id);
        
        if (err == KVError::OK) {
            // Success! Return the new version
            return {new_version, true};
        } else if (err == KVError::ErrVersion) {
            // Version conflict, retry the optimistic concurrency loop
            continue;
        } else if (err == KVError::ErrMaybe) {
            // Network or temporary error, retry
            continue;
        } else {
            throw std::runtime_error("Unexpected error in OnePut: " + ErrorToString(err));
        }
    }
}

// Static utility functions
std::string KVTestFramework::ErrorToString(KVError err) {
    switch (err) {
    case KVError::OK: return "OK";
    case KVError::ErrNoKey: return "ErrNoKey";
    case KVError::ErrVersion: return "ErrVersion";
    case KVError::ErrMaybe: return "ErrMaybe";
    default: return "Unknown";
    }
}

std::string KVTestFramework::RandValue(int length) {
    const std::string charset = 
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.length() - 1);
    
    std::string result;
    result.reserve(static_cast<size_t>(length));
    
    for (int i = 0; i < length; ++i) {
        result += charset[static_cast<size_t>(dis(gen))];
    }
    
    return result;
}

size_t KVTestFramework::GetHeapUsage() {
#ifdef __linux__
    std::ifstream status("/proc/self/status");
    std::string line;
    
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            
            size_t memory_kb = std::stoull(value);
            return memory_kb * 1024; // Convert to bytes
        }
    }
#elif defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss; // Already in bytes on macOS
    }
#endif
    
    return 0; // Fallback - unable to determine memory usage
}
