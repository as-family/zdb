#include "ExponentialBackoff.hpp"
#include "RetryPolicy.hpp"

#include <algorithm>
#include <optional>
#include <chrono>

namespace zdb {

ExponentialBackoff::ExponentialBackoff(const RetryPolicy& P)
    : Policy(P), attempt(0) {}

std::optional<std::chrono::microseconds> ExponentialBackoff::nextDelay() {
    if (attempt >= Policy.failureThreshold) {
        return std::nullopt;
    }
    auto baseCount = static_cast<unsigned long>(Policy.baseDelay.count());
    auto delay = baseCount * (1UL << static_cast<unsigned int>(attempt));
    attempt++;
    return std::chrono::microseconds(std::min(delay, static_cast<unsigned long>(Policy.maxDelay.count())));
}

void ExponentialBackoff::reset() {
    attempt = 0;
}

} // namespace zdb
