#include "CircuitBreaker.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"

namespace zdb {

CircuitBreaker::CircuitBreaker(const RetryPolicy& p)
    : state {State::Closed}, policy{p}, repeater {p}, lastFailureTime{} {}

grpc::Status CircuitBreaker::call(std::function<grpc::Status()>& rpc) {
    switch (state) {
        case State::Open:
            if (std::chrono::steady_clock::now() - lastFailureTime < policy.resetTimeout) {
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
}

bool CircuitBreaker::isOpen() {
    return state == CircuitBreaker::State::Open;
}

} // namespace zdb
