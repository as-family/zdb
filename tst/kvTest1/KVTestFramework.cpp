#include "KVTestFramework.hpp"
#include <iostream>
#include <future>
#include <algorithm>
#include <thread>
#include <cstdlib>
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

std::map<std::string, std::vector<PorcupineOperation>> PorcupineChecker::PartitionByKey() const {
    std::map<std::string, std::vector<PorcupineOperation>> partitioned;
    std::lock_guard<std::mutex> lock(ops_mutex);
    
    for (const auto& op : operations) {
        partitioned[op.input.key].push_back(op);
    }
    
    return partitioned;
}

bool PorcupineChecker::CheckLinearizability(std::chrono::seconds timeout) {
    auto partitioned = PartitionByKey();
    
    // Check each partition independently
    for (const auto& [key, ops] : partitioned) {
        if (!CheckPartitionLinearizability(ops, timeout)) {
            std::cerr << "Linearizability check failed for key: " << key << std::endl;
            return false;
        }
    }
    return true;
}

std::pair<bool, KVState> PorcupineChecker::StepFunction(const KVState& state, 
                                                       const KVInput& input, 
                                                       const KVOutput& output) {
    switch (input.op) {
    case OpType::GET:
        // For GET operations, check if output matches current state
        return {output.value == state.value && output.version == state.version, state};
        
    case OpType::PUT:
        if (state.version == input.version) {
            // Version matches - PUT should succeed or return ErrMaybe
            bool valid = (output.err == "OK" || output.err == "ErrMaybe");
            if (valid && output.err == "OK") {
                return {true, KVState(input.value, state.version + 1)};
            } else {
                // ErrMaybe - could have succeeded or not
                return {true, state}; // Keep current state for ErrMaybe
            }
        } else {
            // Version mismatch - should return ErrVersion or ErrMaybe
            bool valid = (output.err == "ErrVersion" || output.err == "ErrMaybe");
            return {valid, state};
        }
    }
    return {false, state};
}

bool PorcupineChecker::CheckPartitionLinearizability(const std::vector<PorcupineOperation>& ops, 
                                                    std::chrono::seconds /* timeout */) {
    // Simplified linearizability checker
    // For a full implementation, this would need a more sophisticated algorithm
    // that explores all possible interleavings of concurrent operations
    
    if (ops.empty()) return true;
    
    // Sort operations by call time to get a sequential execution
    auto sorted_ops = ops;
    std::sort(sorted_ops.begin(), sorted_ops.end(), 
              [](const PorcupineOperation& a, const PorcupineOperation& b) {
                  return a.call_time < b.call_time;
              });
    
    KVState current_state;
    
    for (const auto& op : sorted_ops) {
        auto [valid, new_state] = StepFunction(current_state, op.input, op.output);
        if (!valid) {
            std::cerr << "Invalid operation: " << op.input.key 
                      << " op=" << static_cast<int>(op.input.op) 
                      << " err=" << op.output.err << std::endl;
            return false;
        }
        current_state = new_state;
    }
    
    return true;
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
    kvStore = std::make_unique<zdb::InMemoryKVStore>();
    serviceImpl = std::make_unique<zdb::KVStoreServiceImpl>(*kvStore);
    server = std::make_unique<zdb::KVStoreServer>(server_address, *serviceImpl);
    serverThread = std::thread([this]() { server->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow server to start
    
    // Create config for clients
    zdb::RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
    std::vector<std::string> addresses{server_address};
    config = std::make_unique<zdb::Config>(addresses, policy);
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
        EntryV entry{client_id, version};
        auto err = PutJson(ck, key, entry, version, client_id);
        
        if (err != KVError::OK && err != KVError::ErrVersion && 
            err != KVError::ErrMaybe) {
            throw std::runtime_error("Unexpected error: " + ErrorToString(err));
        }
        
        // Check what's actually stored
        EntryV stored_entry;
        TVersion stored_version;
        try {
            stored_version = GetJson(ck, key, client_id, stored_entry);
        } catch (const std::runtime_error& e) {
            // Key doesn't exist yet, continue trying
            if (err == KVError::ErrVersion) {
                return {0, false};
            }
            continue;
        }
        
        if (err == KVError::OK && stored_version == version + 1) {
            // Verify it's our put
            if (stored_entry.id != client_id || stored_entry.version != version) {
                throw std::runtime_error("Wrong value stored - expected id=" + 
                                        std::to_string(client_id) + " version=" + 
                                        std::to_string(version) + " but got id=" + 
                                        std::to_string(stored_entry.id) + " version=" + 
                                        std::to_string(stored_entry.version));
            }
            return {stored_version, true};
        } else if (err == KVError::ErrVersion) {
            return {stored_version, false};
        }
        // Continue looping for ErrMaybe
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
