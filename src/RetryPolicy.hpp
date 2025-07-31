#ifndef RETRY_POLICY_H
#define RETRY_POLICY_H

#include <chrono>

namespace zdb {

struct RetryPolicy {
    int failureThreshold;
    std::chrono::microseconds baseDelay;
    std::chrono::microseconds maxDelay;
    std::chrono::microseconds resetTimeout;
};

} // namespace zdb

#endif // RETRY_POLICY_H
