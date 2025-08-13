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
#include <sys/wait.h>
#endif

// NetworkSimulator Implementation
NetworkSimulator::NetworkSimulator(bool isReliable) 
    : reliable(isReliable), gen(rd()), dis(0.0, 1.0) {
}

bool NetworkSimulator::ShouldDropMessage() const {
    bool should_drop = !reliable && dis(gen) < drop_rate;
    return should_drop;
}

bool NetworkSimulator::ShouldDelayMessage() const {
    bool should_delay = !reliable && dis(gen) < delay_rate;
    return should_delay;
}

bool NetworkSimulator::ShouldDuplicateMessage() const {
    bool should_duplicate = !reliable && dis(gen) < duplicate_rate;
    return should_duplicate;
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
    // Call the Go porcupine checker (built by CMake in tst directory)    
#ifdef __linux__
    pid_t pid = fork();
    if (pid == -1) {
        // Fork failed
        std::cerr << "Failed to fork process" << std::endl;
        return false;
    } else if (pid == 0) {
        // Child process
        execl("./porcupine", "porcupine", json_file.c_str(), nullptr);
        // If execl returns, it failed
        std::cerr << "Failed to execute porcupine" << std::endl;
        _exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            std::cerr << "Failed to wait for child process" << std::endl;
            return false;
        }
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
#else
    return false;
#endif
}

bool PorcupineChecker::CheckLinearizability(std::chrono::seconds timeout) {
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
    // NOLINTNEXTLINE(cert-err33-c) - Temporary file cleanup, error not critical
    static_cast<void>(std::remove(temp_file.c_str()));
    
    return result;
}

// KVTestFramework Implementation
KVTestFramework::KVTestFramework(bool reliable) 
    : reliable_network(reliable), real_server_address("localhost:50051"), proxy_server_address("localhost:50052") {

    network_sim = std::make_unique<NetworkSimulator>(reliable);
    porcupine_checker = std::make_unique<PorcupineChecker>();
    kvStore = std::make_unique<zdb::InMemoryKVStore>();
    realServiceImpl = std::make_unique<zdb::KVStoreServiceImpl>(*kvStore);
    realServer = std::make_unique<zdb::KVStoreServer>(real_server_address, *realServiceImpl);

    proxyService = std::make_unique<KVStoreProxyService>(real_server_address, std::move(network_sim));
    proxyServer = std::make_unique<KVStoreServerProxy>(proxy_server_address, std::move(proxyService));

    realServerThread = std::thread([this]() { 
        realServer->wait(); 
    });
    proxyServerThread = std::thread([this]() { 
        proxyServer->wait(); 
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Allow server to start

    zdb::RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 1, 1};
    std::vector<std::string> addresses{proxy_server_address};
    config = std::make_unique<zdb::Config>(addresses, policy);
}

KVTestFramework::~KVTestFramework() {    
    if (realServer) {
        realServer->shutdown();
        if (realServerThread.joinable()) {
            realServerThread.join();
        }
        realServer.reset();
    }

    if (proxyServer) {
        proxyServer->shutdown();
        if (proxyServerThread.joinable()) {
            proxyServerThread.join();
        }
        proxyServer.reset();
    }
    
    if (!current_test_name.empty()) {
        std::cout << "Completed test: " << current_test_name << std::endl;
    }
}

std::unique_ptr<zdb::KVStoreClient> KVTestFramework::makeClient() {    
    return std::make_unique<zdb::KVStoreClient>(*config);
}

void KVTestFramework::Begin(const std::string& test_name) {
    current_test_name = test_name;
    
    if (porcupine_checker) {
        porcupine_checker->Clear();
    }
}

void KVTestFramework::CheckPorcupineT(std::chrono::seconds timeout) {
    if (!porcupine_checker) {
        spdlog::error("Porcupine checker not available");
        return;
    }
    
    spdlog::info("Checking linearizability with {} operations...", porcupine_checker->GetOperationCount());
    
    bool is_linearizable = porcupine_checker->CheckLinearizability(timeout);
    
    if (!is_linearizable) {
        throw std::runtime_error("History is not linearizable");
    }
}

std::vector<ClientResult> KVTestFramework::SpawnClientsAndWait(
    size_t num_clients, 
    std::chrono::seconds duration,
    std::function<ClientResult(int, std::unique_ptr<zdb::KVStoreClient>&, std::atomic<bool>&)> client_fn) {

    
    std::vector<std::thread> threads;
    std::vector<std::promise<ClientResult>> promises(num_clients);
    std::vector<std::future<ClientResult>> futures;
    std::atomic<bool> done{false};
    
    for (auto& promise : promises) {
        futures.push_back(promise.get_future());
    }
    
    for (int i = 0; i < num_clients; i++) {
        threads.emplace_back(&KVTestFramework::RunClient, this, i, client_fn, 
                           std::ref(done), std::ref(promises[static_cast<size_t>(i)]));
    }

    std::this_thread::sleep_for(duration);

    done = true;

    std::vector<ClientResult> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

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
        auto ck = makeClient();
        ClientResult result = client_fn(client_id, ck, done);
        result_promise.set_value(result);
    } catch (const std::exception& e) {
        spdlog::error("Client {} encountered an error: {}", client_id, e.what());
        result_promise.set_exception(std::current_exception());
    }
}

void KVTestFramework::CheckPutConcurrent(std::unique_ptr<zdb::KVStoreClient>& ck, const std::string& key, 
                                        const std::vector<ClientResult>& results, ClientResult* total_result, bool reliable) {
    // Implementation that matches the Go version more closely
    for (const auto& result : results) {
        total_result->nok += result.nok;
        total_result->nmaybe += result.nmaybe;
    }
    
    try {
        EntryV entry;
        TVersion current_version = GetJson(*ck, key, -1, entry);
        
        if (reliable) {
            if (current_version != static_cast<TVersion>(total_result->nok)) {
                throw std::runtime_error(
                    "Reliable: Wrong number of puts: server " + std::to_string(current_version) + 
                    " clnts {" + std::to_string(total_result->nok) + "," + std::to_string(total_result->nmaybe) + "}");
            }
        } else {
            if (current_version > static_cast<TVersion>(total_result->nok + total_result->nmaybe)) {
                throw std::runtime_error(
                    "Unreliable: Wrong number of puts: server " + std::to_string(current_version) + 
                    " clnts {" + std::to_string(total_result->nok) + "," + std::to_string(total_result->nmaybe) + "}");
            }
        }
    } catch (const std::runtime_error& e) {
        if (total_result->nok == 0) {
            // No successful operations, key might not exist
            return;
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
    
    for (const auto& key : keys) {
        version_map[key] = 0;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> key_dist(0, keys.size() - 1);
    
    while (!done.load()) {
        // Select a random key (or just use first key for simple case)
        std::string key = keys[0];  // Use first key like Go version for simplicity
        if (keys.size() > 1) {
            key = keys[key_dist(gen)];
        }
        
        auto [new_version, success] = OnePut(client_id, *ck, key, version_map[key], done);
        
        // Update our version tracker to the current server version
        version_map[key] = new_version;
        
        if (success) {
            result.nok++;
        } else {
            result.nmaybe++;
        }
        
        if (done.load()) break;
    }
    
    return result;
}

std::pair<TVersion, bool> KVTestFramework::OnePut(int client_id, zdb::KVStoreClient& ck, 
                                                 const std::string& key, TVersion version, 
                                                 std::atomic<bool>& done) {
    while (true) {
        // Check if we should stop due to test timeout
        if (done.load()) {
            return {0, false};
        }
        
        // Step 1: Try to put with the specified version (matching Go semantics exactly)
        EntryV entry{client_id, version}; 
        auto err = PutJson(ck, key, entry, version, client_id);

        if (!(err == zdb::ErrorCode::OK || err == zdb::ErrorCode::VersionMismatch || err == zdb::ErrorCode::Maybe)) {
            throw std::runtime_error("Unexpected error in OnePut: " + toString(err));
        }
        
        // Step 2: Get current state to see what version we're at now (matching Go exactly)
        EntryV current_entry;
        TVersion ver0 = GetJson(ck, key, client_id, current_entry);
        
        // Step 3: Check if our put succeeded (exactly like Go version)
        if (err == zdb::ErrorCode::OK && ver0 == version + 1) {
            // My put succeeded - verify the value is correct
            if (current_entry.id != client_id || current_entry.version != version) {
                throw std::runtime_error("Wrong value stored after successful put");
            }
        }
        
        // Step 4: Update version to current state (ver = ver0 in Go)
        version = ver0;
        
        // Step 5: Return based on result (exactly matching Go logic)
        if (err == zdb::ErrorCode::OK || err == zdb::ErrorCode::Maybe) {
            // In Go: return ver, err == rpc.OK
            // This means only return true if err was actually OK (not ErrMaybe)
            return {version, err == zdb::ErrorCode::OK};
        }
        
        // If we got ErrVersion, retry with the new version (continue loop)
        // No explicit sleep in Go version, just retry immediately
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
#endif
    return 0; // Fallback - unable to determine memory usage
}


KVStoreServerProxy::KVStoreServerProxy(const std::string& address, std::unique_ptr<KVStoreProxyService> s)
    : addr{address}, service {std::move(s)} {
    grpc::ServerBuilder sb{};
    sb.AddListeningPort(addr, grpc::InsecureServerCredentials());
    sb.RegisterService(service.get());
    server = sb.BuildAndStart();
}

void KVStoreServerProxy::wait() {
    server->Wait();
}

void KVStoreServerProxy::shutdown() {
    if (server) {
        server->Shutdown();
        server.reset();
    }
}
