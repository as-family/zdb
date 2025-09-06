#include "GoRPCClient.hpp"
#include <iostream>
#include <memory>

GoRPCClient::GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h)
    : i {ii}, address {a}, policy {p}, handle {h} {}

void GoRPCClient::stop() {
    decltype(breakers) local;
    {
        std::unique_lock lock{m};
        local = std::move(breakers);
        breakers.clear();
    }
    for (auto& b : local) {
        b->stop();
    }
    // std::cerr << "C++: GoRPCClient to " << address << " is stopping\n";
}

GoRPCClient::~GoRPCClient() {
    stop();
}
