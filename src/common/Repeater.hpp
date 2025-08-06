#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include <optional>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "RetryPolicy.hpp"
#include "ExponentialBackoff.hpp"
#include "FullJitter.hpp"

namespace zdb {

class Repeater {
public:
    Repeater(const RetryPolicy& P);
    grpc::Status attempt(const std::function<grpc::Status()>& rpc);
private:
    ExponentialBackoff backoff;
    FullJitter fullJitter;
};

} // namespace zdb

#endif // REPEATER_H
