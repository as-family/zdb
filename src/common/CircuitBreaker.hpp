#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include <functional>
#include <grpcpp/grpcpp.h>
#include "RetryPolicy.hpp"
#include "Repeater.hpp"

namespace zdb {

class CircuitBreaker {
public:
    enum class State {
        Open,
        Closed,
        HalfOpen
    };
    CircuitBreaker(const RetryPolicy& P);
    grpc::Status call(const std::function<grpc::Status()>& rpc);
    bool open() const;
private:
    State state;
    const RetryPolicy& Policy;
    Repeater repeater;
    std::chrono::steady_clock::time_point lastFailureTime;
};

} // namespace zdb

#endif // CIRCUIT_BREAKER_H
