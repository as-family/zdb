#include "ExponentialBackoff.hpp"
#include "RetryPolicy.hpp"

#include <algorithm>
#include <optional>
#include <chrono>
#include <cstdint>
#include <spdlog/spdlog.h>

namespace zdb {

ExponentialBackoff::ExponentialBackoff(const RetryPolicy p)
    : policy {p} {}

std::optional<std::chrono::microseconds> ExponentialBackoff::nextDelay() {
    if (attempt >= policy.failureThreshold - 1) {
        return std::nullopt;
    }
    auto baseCount = static_cast<uint64_t>(policy.baseDelay.count());
    auto delay = baseCount * (1UL << static_cast<unsigned int>(attempt));
    attempt++;
    spdlog::info("ExponentialBackoff: Attempt {}, delay: {}, maxDelay: {}", attempt, delay, policy.maxDelay.count());
    return std::chrono::microseconds(std::min(delay, static_cast<uint64_t>(policy.maxDelay.count())));
}

void ExponentialBackoff::reset() {
    attempt = 0;
}

} // namespace zdb
