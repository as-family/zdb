#include "ExponentialBackoff.hpp"
#include "RetryPolicy.hpp"

#include <algorithm>
#include <optional>
#include <chrono>

namespace zdb {

ExponentialBackoff::ExponentialBackoff(const RetryPolicy& p)
    : policy(p), attempt(0) {}

std::optional<std::chrono::microseconds> ExponentialBackoff::nextDelay() {
    if (attempt >= policy.failureThreshold) {
        return std::nullopt;
    }
    auto baseCount = static_cast<unsigned long>(policy.baseDelay.count());
    auto delay = baseCount * (1UL << static_cast<unsigned int>(attempt));
    attempt++;
    return std::chrono::microseconds(std::min(delay, static_cast<unsigned long>(policy.maxDelay.count())));
}

void ExponentialBackoff::reset() {
    attempt = 0;
}

} // namespace zdb
