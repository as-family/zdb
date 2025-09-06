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
#include <string>

namespace zdb {

Repeater::Repeater(const RetryPolicy p)
    : backoff {p} {}

std::vector<grpc::Status> Repeater::attempt(const std::string& op, const std::function<grpc::Status()>& rpc) {
    std::vector<grpc::Status> statuses;
    while (!stopped.load(std::memory_order_acquire)) {
        auto status = rpc();
        if (stopped.load(std::memory_order_acquire)) {
            statuses.push_back(grpc::Status{grpc::StatusCode::CANCELLED, "Repeater stopped"});
            return statuses;
        }
        statuses.push_back(status);
        if (status.ok()) {
            backoff.reset();
            return statuses;
        } else {
            if (!isRetriable(op, toError(status).code)) {
                backoff.reset();
                return statuses;
            }
            auto delay = backoff.nextDelay()
                .and_then([this](std::chrono::microseconds v) {
                    return std::optional<std::chrono::microseconds> {fullJitter.jitter(v)};
                });

            if (delay.has_value()) {
                auto remaining = delay.value();
                while (!stopped.load(std::memory_order_acquire) && remaining > std::chrono::microseconds::zero()) {
                    auto step = std::min<std::chrono::microseconds>(remaining, std::chrono::microseconds{1000});
                    std::this_thread::sleep_for(step);
                    remaining -= step;
                }
                if (stopped.load(std::memory_order_acquire)) {
                    statuses.push_back(grpc::Status{grpc::StatusCode::CANCELLED, "Repeater stopped"});
                    return statuses;
                }
            } else {
                return statuses;
            }
        }
    }
    statuses.push_back(grpc::Status{grpc::StatusCode::CANCELLED, "Repeater stopped"});
    return statuses;
}

void Repeater::reset() {
    stopped.store(false, std::memory_order_release);
    backoff.reset();
}

void Repeater::stop() noexcept {
    stopped = true;
}

} // namespace zdb
