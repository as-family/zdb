#include "Repeater.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"
#include "RetryPolicy.hpp"
#include <functional>
#include <optional>
#include <chrono>
#include <thread>
#include <grpcpp/support/status.h>

namespace zdb {

Repeater::Repeater(const RetryPolicy& P)
    : backoff {P} {}

grpc::Status Repeater::attempt(const std::function<grpc::Status()>& rpc) {
    while (true) {
        auto status = rpc();
        if (status.ok()) {
            backoff.reset();
            return status;
        } else {
            if (!isRetriable(toError(status).code)) {
                backoff.reset();
                return status;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });
            if (delay.has_value()) {
                std::this_thread::sleep_for(delay.value());
            } else {
                return status;
            }
        }
    }
}

} // namespace zdb
