#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "server/InMemoryKVStore.hpp"
#include "server/KVStoreServer.hpp"
#include "client/KVStoreClient.hpp"
#include "common/RetryPolicy.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <string>

using namespace zdb;

int main(int argc, char** argv) {
    std::string peer_id {"Alice"};
    std::string port {"50051"};
    std::string listen_address {"localhost:" + port};
    std::vector<std::string> peer_addresses {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"
    };
    
    InMemoryKVStore kvStore {};
    KVStoreServiceImpl s{kvStore};
    KVStoreServer ss {listen_address, s};
    std::this_thread::sleep_for(std::chrono::seconds(1));

    RetryPolicy p {std::chrono::microseconds(100), std::chrono::milliseconds(500), std::chrono::seconds(5), 3};
    KVStoreClient client {peer_addresses, p};
    client.set("hello", "world");
    std::cout << client.get("hello").value() << std::endl;
    std::cout << client.size().value() << std::endl;
    std::cout << client.erase("hello").value() << std::endl;
    std::cout << client.size().value() << std::endl;
    std::cout << client.get("hello").error().what << std::endl;
    return 0;
}
