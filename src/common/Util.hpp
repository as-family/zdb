#ifndef ZDB_UTIL_H_DLKJH
#define ZDB_UTIL_H_DLKJH

#include <cstring>
#include <random>
#include <array>
#include <algorithm>
#include <functional>
#include <string>

template <typename T = std::mt19937>
auto random_generator() -> T {
    auto constexpr seed_bytes = sizeof(typename T::result_type) * T::state_size;
    auto constexpr seed_len = seed_bytes / sizeof(std::seed_seq::result_type);
    auto seed = std::array<std::seed_seq::result_type, seed_len>();
    auto dev = std::random_device();
    std::generate_n(begin(seed), seed_len, std::ref(dev));
    auto seed_seq = std::seed_seq(begin(seed), end(seed));
    return T{seed_seq};
}

std::string zdb_generate_random_alphanumeric_string(std::size_t len);

#endif // ZDB_UTIL_H_DLKJH
