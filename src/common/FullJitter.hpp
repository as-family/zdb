#ifndef FULL_JITTER_H
#define FULL_JITTER_H

#include <random>
#include <chrono>

namespace zdb {

class FullJitter {
public:
    FullJitter();
    std::chrono::microseconds jitter(const std::chrono::microseconds v);
private:
    std::mt19937 rng;
};

} // namespace zdb

#endif // FULL_JITTER_H
