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
    int services,
    std::chrono::milliseconds rpc,
    std::chrono::milliseconds channel)
    : baseDelay(base),
      maxDelay(max),
      resetTimeout(reset),
      failureThreshold(threshold),
      servicesToTry(services),
      rpcTimeout(rpc),
      channelTimeout(channel) {
    if (threshold < 0) {
        throw std::invalid_argument("Failure threshold must be >= zero.");
    }
    if (services < 0) {
        throw std::invalid_argument("Services to try must be >= zero.");
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
    if (rpc < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("RPC timeout must be >= zero.");
    }
    if (channel < std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Channel timeout must be >= zero.");
    }
    if (max < base) {
        throw std::invalid_argument("Max delay must be >= base delay.");
    }
}

} // namespace zdb
