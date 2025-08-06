#include "CircuitBreaker.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"
#include <spdlog/spdlog.h>
#include "common/RetryPolicy.hpp"
#include <grpcpp/support/status.h>
#include <functional>
#include <chrono>
#include <utility>

namespace zdb {

CircuitBreaker::CircuitBreaker(const RetryPolicy& P)
    : state {State::Closed}, Policy{P}, repeater {P} {}

grpc::Status CircuitBreaker::call(const std::function<grpc::Status()>& rpc) {
    if (rpc == nullptr) {
        spdlog::error("CircuitBreaker: rpc function is nullptr. Throwing bad_function_call.");
        throw std::bad_function_call {};
    }
    switch (state) {
        case State::Open:
            if (std::chrono::steady_clock::now() - lastFailureTime < Policy.resetTimeout) {
                return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Circuit breaker is open");
            }
            state = State::HalfOpen;
            [[fallthrough]];
        case State::HalfOpen:
        {
            auto status = rpc();
            if (status.ok()) {
                state = State::Closed;
            } else {
                if (isRetriable(toError(status).code)) {
                    state = State::Open;
                    lastFailureTime = std::chrono::steady_clock::now();
                } else {
                    state = State::Closed;
                }
            }
            return status;
        }
        case State::Closed:
        {
            auto status = repeater.attempt(rpc);
            if (!status.ok()) {
                if (isRetriable(toError(status).code)) {
                    state = State::Open;
                    lastFailureTime = std::chrono::steady_clock::now();
                }
            }
            return status;
        }
    }
    std::unreachable();
}

bool CircuitBreaker::open() const {
    return state == CircuitBreaker::State::Open;
}

} // namespace zdb
