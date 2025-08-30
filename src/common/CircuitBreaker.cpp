#include "common/CircuitBreaker.hpp"
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <spdlog/spdlog.h>
#include "common/RetryPolicy.hpp"
#include <grpcpp/support/status.h>
#include <functional>
#include <chrono>
#include <utility>
#include <vector>
#include <string>

namespace zdb {

CircuitBreaker::CircuitBreaker(const RetryPolicy p)
    : state{State::Closed},
      policy{p},
      repeater{p},
      lastFailureTime{} {}

std::vector<grpc::Status> CircuitBreaker::call(const std::string& op, const std::function<grpc::Status()>& rpc) {
    if (rpc == nullptr) {
        throw std::bad_function_call {};
    }
    switch (state) {
        case State::Open:
            if (std::chrono::steady_clock::now() - lastFailureTime < policy.resetTimeout) {
                return {grpc::Status(grpc::StatusCode::UNAVAILABLE, "Circuit breaker is open")};
            }
            state = State::HalfOpen;
            [[fallthrough]];
        case State::HalfOpen:
        {
            auto status = rpc();
            if (status.ok()) {
                state = State::Closed;
                repeater.reset();
            } else {
                if (isRetriable(op, toError(status).code)) {
                    state = State::Open;
                    lastFailureTime = std::chrono::steady_clock::now();
                } else {
                    state = State::Closed;
                }
            }
            return {status};
        }
        case State::Closed:
        {
            auto statuses = repeater.attempt(op, rpc);
            if (!statuses.back().ok()) {
                if (isRetriable(op, toError(statuses.back()).code)) {
                    state = State::Open;
                    lastFailureTime = std::chrono::steady_clock::now();
                }
            }
            return statuses;
        }
    }
    std::unreachable();
}

bool CircuitBreaker::open() {
    if (state == State::Open && std::chrono::steady_clock::now() - lastFailureTime >= policy.resetTimeout) {
        state = State::HalfOpen;
    }
    return state == CircuitBreaker::State::Open;
}

} // namespace zdb
