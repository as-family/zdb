#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "InMemoryKVStore.hpp"
#include "KVStoreServer.hpp"
#include "KVStoreClient.hpp"

using namespace zdb;

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <peer_id> <port>" << std::endl;
        return 1;
    }

    std::string peer_id {argv[1]};
    std::string port {argv[2]};
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

    KVStoreClient client {listen_address};
    client.set("hello", "world");
    std::cout << client.get("hello")->value() << std::endl;
    return 0;
}
