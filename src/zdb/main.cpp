#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include "server/InMemoryKVStore.hpp"
#include "server/KVStoreServer.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/RetryPolicy.hpp"
#include <spdlog/spdlog.h>
#include "common/Types.hpp"

using zdb::Key;
using zdb::InMemoryKVStore;
using zdb::KVStoreServiceImpl;
using zdb::KVStoreServer;
using zdb::KVStoreClient;
using zdb::Config;
using zdb::RetryPolicy;

int main(int /*argc*/, char** /*argv*/) {
    spdlog::info("ZDB! Starting...");
    const std::string port {"50051"};
    const std::string listenAddress {"localhost:" + port};
    spdlog::info("Listening on: {}", listenAddress);
    const std::vector<std::string> peerAddresses {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"
    };

    InMemoryKVStore kvStore {};
    KVStoreServiceImpl s{kvStore};
    const KVStoreServer server {listenAddress, s};
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const RetryPolicy policy {std::chrono::microseconds(100), std::chrono::milliseconds(500), std::chrono::seconds(5), 3, 3};
    Config config {peerAddresses, policy};
    const KVStoreClient client {config};
    std::cout << client.get(Key{"hello"}).error().what << '\n';
    return 0;
}
