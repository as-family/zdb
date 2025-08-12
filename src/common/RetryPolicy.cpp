#include "RetryPolicy.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>

namespace zdb {

RetryPolicy::RetryPolicy(
    std::chrono::microseconds base,
    std::chrono::microseconds max,
    std::chrono::microseconds reset,
    int threshold,
    int services) 
    : baseDelay(base), maxDelay(max), resetTimeout(reset), failureThreshold(threshold), servicesToTry(services) {
    if (threshold < 0) {
        spdlog::error("RetryPolicy: Failure threshold must be >= zero. Throwing invalid_argument.");
        throw std::invalid_argument("Failure threshold must be >= zero.");
    }
    if (services < 0) {
        spdlog::error("RetryPolicy: Services to try must be > zero. Throwing invalid_argument.");
        throw std::invalid_argument("Services to try must be > zero.");
    }
    if (base < std::chrono::microseconds::zero()) {
        spdlog::error("RetryPolicy: Base delay must be >= zero. Throwing invalid_argument.");
        throw std::invalid_argument("Base delay must be >= zero.");
    }
    if (max < std::chrono::microseconds::zero()) {
        spdlog::error("RetryPolicy: Max delay must be >= zero. Throwing invalid_argument.");
        throw std::invalid_argument("Max delay must be >= zero.");
    }
    if (reset < std::chrono::microseconds::zero()) {
        spdlog::error("RetryPolicy: Reset timeout must be >= zero. Throwing invalid_argument.");
        throw std::invalid_argument("Reset timeout must be >= zero.");
    }
    if (max < base) {
        spdlog::error("RetryPolicy: Max delay must be >= base delay. Throwing invalid_argument.");
        throw std::invalid_argument("Max delay must be >= base delay.");
    }
}

} // namespace zdb
