#include "GoRPCClient.hpp"
#include <iostream>

GoRPCClient::GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h)
    : i {ii}, address {a}, policy {p}, handle {h} {}

void GoRPCClient::stop() {
    for (auto& b : breakers) {
        b.get().stop();
    }
    std::cerr << "C++: GoRPCClient to " << address << " is stopping\n";
    std::unique_lock lock{m};
    breakers.clear();
    stopped = true;
}

GoRPCClient::~GoRPCClient() {
    if (!stopped) {
        stop();
    }
}
