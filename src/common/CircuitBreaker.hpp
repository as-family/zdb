#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include <functional>
#include "RetryPolicy.hpp"
#include "Repeater.hpp"
#include <grpcpp/support/status.h>
#include <vector>
#include <string>
#include <chrono>

namespace zdb {

class CircuitBreaker {
public:
    enum class State : char {
        Open,
        Closed,
        HalfOpen
    };
    explicit CircuitBreaker(const RetryPolicy p);
    std::vector<grpc::Status> call(const std::string& op, const std::function<grpc::Status()>& rpc);
    [[nodiscard]] bool open();
private:
    State state;
    RetryPolicy policy;
    Repeater repeater;
    std::chrono::steady_clock::time_point lastFailureTime;
};

} // namespace zdb

#endif // CIRCUIT_BREAKER_H
