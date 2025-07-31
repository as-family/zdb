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
    CircuitBreaker(const RetryPolicy& p);
    grpc::Status call(std::function<grpc::Status()>& rpc);
    bool isOpen();
private:
    State state;
    const RetryPolicy& policy;
    Repeater repeater;
    std::chrono::steady_clock::time_point lastFailureTime;
};

} // namespace zdb

#endif // CIRCUIT_BREAKER_H
