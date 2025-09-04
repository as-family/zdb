#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include "RetryPolicy.hpp"
#include "ExponentialBackoff.hpp"
#include "FullJitter.hpp"
#include <grpcpp/support/status.h>
#include <vector>
#include <string>
#include <atomic>

namespace zdb {

class Repeater {
public:
    explicit Repeater(const RetryPolicy p);
    std::vector<grpc::Status> attempt(const std::string& op, const std::function<grpc::Status()>& rpc);
    void reset();
    void stop();
private:
    ExponentialBackoff backoff;
    FullJitter fullJitter;
    std::atomic<bool> stopped{false};
};

} // namespace zdb

#endif // REPEATER_H
