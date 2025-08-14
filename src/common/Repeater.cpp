#include "Repeater.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"
#include "RetryPolicy.hpp"
#include <functional>
#include <optional>
#include <chrono>
#include <thread>
#include <grpcpp/support/status.h>
#include <spdlog/spdlog.h>

namespace zdb {

Repeater::Repeater(const RetryPolicy p)
    : backoff {p} {}

grpc::Status Repeater::attempt(const std::function<grpc::Status()>& rpc) {
    grpc::Status initialStatus = rpc();
    auto status = initialStatus;
    while (true) {
        if (status.ok()) {
            backoff.reset();
            return status;
        } else {
            if (!isRetriable(toError(status).code)) {
                backoff.reset();
                if (isRetriable(toError(initialStatus).code)) {
                    return toGrpcStatus(Error(ErrorCode::Maybe, "Maybe success"));
                } else {
                    return status;
                }
                return status;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });
            
            if (delay.has_value()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            } else {
                return status;
            }
        }
        status = rpc();
    }
}

} // namespace zdb
