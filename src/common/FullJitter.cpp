#include "FullJitter.hpp"

namespace zdb {

FullJitter::FullJitter() : rng(std::random_device{}()) {}

std::chrono::microseconds FullJitter::jitter(std::chrono::microseconds v) {
    if (v < std::chrono::microseconds(0)) {
        throw std::invalid_argument("Negative duration is not supported");
    }
    std::uniform_int_distribution<int> dist(0, v.count());
    return std::chrono::microseconds(dist(rng));
}

} // namespace zdb
