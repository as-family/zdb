#include "ExponentialBackoff.hpp"

#include <algorithm>

namespace zdb {

ExponentialBackoff::ExponentialBackoff(const RetryPolicy& p)
    : policy(p), attempt(0) {}

std::optional<std::chrono::microseconds> ExponentialBackoff::nextDelay() {
    if (attempt >= policy.failureThreshold) {
        return std::nullopt;
    }
    auto delay = policy.baseDelay.count() * (1 << attempt);
    attempt++;
    return std::chrono::microseconds(std::min(delay, policy.maxDelay.count()));
}

void ExponentialBackoff::reset() {
    attempt = 0;
}

} // namespace zdb
