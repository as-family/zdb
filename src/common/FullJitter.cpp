#include "FullJitter.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <stdexcept>
#include <random>

namespace zdb {

FullJitter::FullJitter() : rng(std::random_device{}()) {}

std::chrono::microseconds FullJitter::jitter(const std::chrono::microseconds v) {
    if (v < std::chrono::microseconds(0)) {
        spdlog::error("FullJitter: Negative duration is not supported. Throwing invalid_argument.");
        throw std::invalid_argument("Negative duration is not supported");
    }
    std::uniform_int_distribution<long> dist(0, v.count());
    return std::chrono::microseconds(dist(rng));
}

} // namespace zdb
