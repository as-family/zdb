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
            spdlog::info("Repeater: Status OK:  {}", status.error_message());
            backoff.reset();
            return status;
        } else {
            spdlog::warn("Repeater: Status:  {}", status.error_message());
            if (!isRetriable(toError(status).code)) {
                backoff.reset();
                return status;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });
            
            if (delay.has_value()) {
                spdlog::warn("Repeater: delaying:  {}", delay.value().count());
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            } else {
                if (initialStatus.error_code() == status.error_code()) {
                    return status;
                } else {
                    spdlog::warn("Repeater: changing to Maybe:  {}", status.error_message());
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
