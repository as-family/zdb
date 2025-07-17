#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include "PeerNode.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <peer_id> <port>" << std::endl;
        return 1;
    }

    std::string peer_id = argv[1];
    std::string port = argv[2];
    std::string listen_address = "localhost:" + port;

    std::vector<std::string> peer_addresses = {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"
    };

    PeerNode peer(peer_id, listen_address, peer_addresses);
    peer.startServer();

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    peer.pingPong();

    return 0;
}
