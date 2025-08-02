#include "RetryPolicy.hpp"

namespace zdb {

RetryPolicy::RetryPolicy(std::chrono::microseconds base, std::chrono::microseconds max, std::chrono::microseconds reset, int threshold) {
    if (threshold < 0) {
        throw std::invalid_argument("Failure threshold must be >= zero.");
    }
    if (base < std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Base delay must be >= zero.");
    }
    if (max < std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Max delay must be >= zero.");
    }
    if (reset < std::chrono::microseconds::zero()) {
        throw std::invalid_argument("Reset timeout must be >= zero.");
    }
    if (max < base) {
        throw std::invalid_argument("Max delay must be >= base delay.");
    }
    baseDelay = base;
    maxDelay = max;
    resetTimeout = reset;
    failureThreshold = threshold;
}

} // namespace zdb
