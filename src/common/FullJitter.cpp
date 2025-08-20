#include "FullJitter.hpp"
#include <chrono>
#include <stdexcept>
#include <random>

namespace zdb {

FullJitter::FullJitter() : rng(std::random_device{}()) {}

std::chrono::microseconds FullJitter::jitter(const std::chrono::microseconds v) {
    if (v < std::chrono::microseconds(0)) {
        throw std::invalid_argument("Negative duration is not supported");
    }
    std::uniform_int_distribution<std::chrono::microseconds::rep> dist(0, v.count());
    return std::chrono::microseconds(dist(rng));
}

} // namespace zdb
