#ifndef REPEATER_H
#define REPEATER_H

#include <functional>
#include "RetryPolicy.hpp"
#include "ExponentialBackoff.hpp"
#include "FullJitter.hpp"
#include <grpcpp/support/status.h>
#include <vector>
#include <string>

namespace zdb {

class Repeater {
public:
    explicit Repeater(const RetryPolicy p);
    std::vector<grpc::Status> attempt(std::string op, const std::function<grpc::Status()>& rpc);
private:
    ExponentialBackoff backoff;
    FullJitter fullJitter;
};

} // namespace zdb

#endif // REPEATER_H
