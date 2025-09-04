#include "GoRPCClient.hpp"

GoRPCClient::GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h)
    : i {ii}, address {a}, policy {p}, handle {h} {}

void GoRPCClient::stop() {
    for (auto& b : breakers) {
        b.get().stop();
    }
    breakers.clear();
}

GoRPCClient::~GoRPCClient() {
    stop();
}
