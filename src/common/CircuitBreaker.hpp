#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include <functional>
#include "RetryPolicy.hpp"
#include "Repeater.hpp"
#include <grpcpp/support/status.h>

namespace zdb {

class CircuitBreaker {
public:
    enum class State : char {
        Open,
        Closed,
        HalfOpen
    };
    explicit CircuitBreaker(const RetryPolicy p);
    grpc::Status call(const std::function<grpc::Status()>& rpc);
    [[nodiscard]] bool open();
private:
    State state{State::Closed};
    const RetryPolicy policy;
    Repeater repeater;
    std::chrono::steady_clock::time_point lastFailureTime;
};

} // namespace zdb

#endif // CIRCUIT_BREAKER_H
