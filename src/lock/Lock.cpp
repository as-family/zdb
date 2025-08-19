#include "lock/Lock.hpp"
#include "common/Types.hpp"
#include <random>
#include <string>
#include <algorithm>
#include <mutex>

namespace zdb {

Lock::Lock(const Key& key, KVStoreClient& c) : lock_key(key), client(c), m{}, clientID{} {}

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

auto generate_random_alphanumeric_string(std::size_t len) -> std::string {
    static constexpr auto chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    thread_local auto rng = random_generator<>();
    auto dist = std::uniform_int_distribution{{}, std::strlen(chars) - 1};
    auto result = std::string(len, '\0');
    std::generate_n(begin(result), len, [&]() { return chars[dist(rng)]; });
    return result;
}

bool Lock::wait(std::string c, uint64_t version) {
    while(true) {
        auto v = client.set(lock_key, Value{c, version});
        if (v.has_value()) {
            return true;
        } else {
            if (waitGet(c, version + 1)) {
                return true;
            }
        }
    }
    std::unreachable();
}

bool Lock::waitGet(std::string c, uint64_t version) {
    while (true) {
        auto t = client.get(lock_key);
        if(t.has_value()) {
            if(t.value().version == version) {
                if (t.value().data == c) {
                    return true;
                } else {
                    return false;
                }
            }
        } else {
            if (t.error().code == ErrorCode::KeyNotFound) {
                return false;
            }
        }
    }
    std::unreachable();
}

bool Lock::acquire() {
    auto c = generate_random_alphanumeric_string(16);
    return wait(c, 0);
}

bool Lock::release() {
    auto c = generate_random_alphanumeric_string(16);
    wait(c, 1);
    while(true) {
        auto v = client.erase(lock_key);
        if (v.has_value()) {
            return true;
        } else {
            if (!waitGet(c, 2)) {
                return true;
            }
        }
    }
    std::unreachable();
}

} // namespace zdb
