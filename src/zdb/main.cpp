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

using namespace zdb;

int main(int /*argc*/, char** /*argv*/) {
    spdlog::info("ZDB! Starting...");
    const std::string Port {"50051"};
    const std::string ListenAddress {"localhost:" + Port};
    spdlog::info("Listening on: {}", ListenAddress);
    const std::vector<std::string> PeerAddresses {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"
    };

    InMemoryKVStore kvStore {};
    KVStoreServiceImpl s{kvStore};
    const KVStoreServer Server {ListenAddress, s};
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const RetryPolicy Policy {std::chrono::microseconds(100), std::chrono::milliseconds(500), std::chrono::seconds(5), 3, 3};
    Config config {PeerAddresses, Policy};
    const KVStoreClient Client {config};
    std::cout << Client.get("hello").error().what << '\n';
    return 0;
}
