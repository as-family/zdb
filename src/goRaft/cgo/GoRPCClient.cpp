#include "GoRPCClient.hpp"
#include <iostream>
#include <memory>

GoRPCClient::GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h)
    : i {ii}, address {a}, policy {p}, handle {h} {}

void GoRPCClient::stop() {
    std::unique_lock lock{m};
    for (auto& b : breakers) {
        b.get().stop();
    }
    // std::cerr << "C++: GoRPCClient to " << address << " is stopping\n";
    breakers.clear();
}

GoRPCClient::~GoRPCClient() {
    stop();
}
