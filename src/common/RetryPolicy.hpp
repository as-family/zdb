#ifndef RETRY_POLICY_H
#define RETRY_POLICY_H

#include <chrono>

namespace zdb {

struct RetryPolicy {
    RetryPolicy(
        std::chrono::microseconds base,
        std::chrono::microseconds max,
        std::chrono::microseconds reset,
        int threshold,
        int services,
        std::chrono::milliseconds rpc,
        std::chrono::milliseconds channel
    );
    std::chrono::microseconds baseDelay;
    std::chrono::microseconds maxDelay;
    std::chrono::microseconds resetTimeout;
    int failureThreshold;
    int servicesToTry;
    std::chrono::milliseconds rpcTimeout;
    std::chrono::milliseconds channelTimeout;
};

} // namespace zdb

#endif // RETRY_POLICY_H
