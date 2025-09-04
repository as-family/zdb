#include "GoRPCClient.hpp"

GoRPCClient::GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h)
        : i {ii}, address {a}, circuitBreaker {p}, handle {h} {}
