#include "ExponentialBackoff.hpp"
#include "RetryPolicy.hpp"

#include <algorithm>
#include <optional>
#include <chrono>
#include <cstdint>

namespace zdb {

ExponentialBackoff::ExponentialBackoff(const RetryPolicy& p)
    : policy {p} {}

std::optional<std::chrono::microseconds> ExponentialBackoff::nextDelay() {
    if (attempt >= policy.failureThreshold) {
        return std::nullopt;
    }
    auto baseCount = static_cast<uint64_t>(policy.baseDelay.count());
    auto delay = baseCount * (1UL << static_cast<unsigned int>(attempt));
    attempt++;
    return std::chrono::microseconds(std::min(delay, static_cast<uint64_t>(policy.maxDelay.count())));
}

void ExponentialBackoff::reset() {
    attempt = 0;
}

} // namespace zdb
