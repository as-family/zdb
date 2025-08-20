#ifndef EXPONENTIAL_BACKOFF_H
#define EXPONENTIAL_BACKOFF_H

#include "RetryPolicy.hpp"
#include <optional>
#include <chrono>

namespace zdb {

class ExponentialBackoff {
public:
    explicit ExponentialBackoff(const RetryPolicy policy);
    std::optional<std::chrono::microseconds> nextDelay();
    void reset();
private:
    RetryPolicy policy;
    int attempt{0};
};

} // namespace zdb

#endif // EXPONENTIAL_BACKOFF_H
