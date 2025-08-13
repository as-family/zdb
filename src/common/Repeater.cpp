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

Repeater::Repeater(const RetryPolicy& p)
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
                return status;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });
            if (delay.has_value()) {
                std::this_thread::sleep_for(delay.value());
            } else {
                if (initialStatus.error_code() == status.error_code()) {
                    return status;
                } else {
                    return grpc::Status(
                        grpc::StatusCode::DATA_LOSS,
                        "Maybe: " + status.error_message()
                    );
                }
            }
        }
        status = rpc();
    }
}

} // namespace zdb
