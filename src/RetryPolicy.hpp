#ifndef RETRY_POLICY_H
#define RETRY_POLICY_H

#include <chrono>

namespace zdb {

struct RetryPolicy {
    RetryPolicy(std::chrono::microseconds base, std::chrono::microseconds max, std::chrono::microseconds reset, int threshold);
    std::chrono::microseconds baseDelay;
    std::chrono::microseconds maxDelay;
    std::chrono::microseconds resetTimeout;
    int failureThreshold;
};

} // namespace zdb

#endif // RETRY_POLICY_H
