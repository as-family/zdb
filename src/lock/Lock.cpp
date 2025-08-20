#include "lock/Lock.hpp"
#include "common/Types.hpp"
#include <random>
#include <string>
#include <algorithm>
#include <mutex>

namespace zdb {

Lock::Lock(const Key key, KVStoreClient& c) : lock_key(key), client(c), m{}, clientID{} {}

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

void Lock::acquire() {
    auto c = generate_random_alphanumeric_string(16);
    client.waitSet(lock_key, Value{c, 0});
}

void Lock::release() {
    auto v = client.waitGet(lock_key, 1);
    client.waitSet(lock_key, v);
    while (true) {
        auto t = client.erase(lock_key);
        if (t.has_value()) {
            return;
        } else {
            if (client.waitNotFound(lock_key)) {
                return;
            } else {
                if (!client.waitGet(lock_key, Value{v.data, 2})) {
                    return;
                }
            }
        }
    }
    std::unreachable();
}

} // namespace zdb
