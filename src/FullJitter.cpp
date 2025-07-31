#include "FullJitter.hpp"

namespace zdb {

FullJitter::FullJitter() : rng(std::random_device{}()) {}

std::chrono::microseconds FullJitter::jitter(std::chrono::microseconds v) {
    std::uniform_int_distribution<int> dist(0, v.count());
    return std::chrono::microseconds(dist(rng));
}

} // namespace zdb
