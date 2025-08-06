#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include <optional>
#include <thread>
#include <chrono>
#include "RetryPolicy.hpp"
#include "ExponentialBackoff.hpp"
#include "FullJitter.hpp"
#include <grpcpp/support/status.h>

namespace zdb {

class Repeater {
public:
    Repeater(const RetryPolicy& p);
    grpc::Status attempt(const std::function<grpc::Status()>& rpc);
private:
    ExponentialBackoff backoff;
    FullJitter fullJitter;
};

} // namespace zdb

#endif // REPEATER_H
