#include "Repeater.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"
#include "RetryPolicy.hpp"
#include <functional>
#include <optional>
#include <chrono>
#include <thread>
#include <grpcpp/support/status.h>
#include <vector>

namespace zdb {

Repeater::Repeater(const RetryPolicy p)
    : backoff {p} {}

std::vector<grpc::Status> Repeater::attempt(const std::function<grpc::Status()>& rpc) {
    std::vector<grpc::Status> statuses;
    while (true) {
        auto status = rpc();
        statuses.push_back(status);
        if (status.ok()) {
            backoff.reset();
            return statuses;
        } else {
            if (!isRetriable(toError(status).code)) {
                backoff.reset();
                return statuses;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });
            
            if (delay.has_value()) {
                std::this_thread::sleep_for(delay.value());
            } else {
                return statuses;
            }
        }
    }
}

} // namespace zdb
