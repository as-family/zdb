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

bool Lock::acquire() {
    auto c = generate_random_alphanumeric_string(16);
    while (true) {
        auto v = client.set(lock_key, Value{c, 0});
        if (v.has_value()) {
            std::cerr << "clientID: " << c << " Acquired lock\n";
            clientID = c;
            return true;
        }
        if (v.error().code == ErrorCode::VersionMismatch) {
            if (v.error().version == 1 && v.error().value == c) {
                std::cerr << "Maybe clientID: " << clientID << "\n";
                clientID = c;
                return true;
            }
        }
        std::cerr << "retry aquire " << v.error().what << " " << c << " " << v.error().value << "\n";
    }
    throw std::runtime_error("Failed to acquire lock");
}

bool Lock::release() {
    auto c = "";
    std::cerr << "Releasing lock for clientID: " << c << "\n";
    while(true) {
        auto s = client.set(lock_key, Value{c, 1});
        if (s.has_value()) {
           break;
        }
        if (s.error().code == ErrorCode::KeyNotFound) {
            throw std::runtime_error("Lock is not held by anyone");
        }
        if (s.error().code == ErrorCode::VersionMismatch) {
            if (s.error().version == 0) {
                throw std::runtime_error("Lock is not held by anyone");
            } else if (s.error().version == 2) {
                break;
            } else {
                throw std::runtime_error("Unexpected version mismatch: " + s.error().what);
            }
        }
        std::cerr << "release front " << s.error().what << "\n";
    }
    while (true) {
        auto v = client.erase(lock_key);
        if (v.has_value()) {
            if (v.value().version == 2) {
                std::cerr << "Second try Lock released successfully\n";
                return true;
            } else if (v.value().version == 1) {
                std::cerr << "some packets dropped\n";
                return true;
            } else {
                throw std::runtime_error("Unexpected version mismatch: " + v.error().what);
            }
        } else {
            if (v.error().code == ErrorCode::KeyNotFound) {
                std::cerr << "Lock already released or never acquired by clientID: " << c << "\n";
                return true;
            } else {
                throw std::runtime_error("Failed to release lock");
            }
        }
        std::cerr << "release back " << v.error().what << "\n";
    }
    throw std::runtime_error("Failed to release lock");
}

} // namespace zdb
