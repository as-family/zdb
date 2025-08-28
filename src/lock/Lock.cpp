#include "lock/Lock.hpp"
#include "common/Types.hpp"
#include <random>
#include <string>
#include <algorithm>
#include "common/Util.hpp"

namespace zdb {

Lock::Lock(const Key key, KVStoreClient& c) : lockKey(key), client(c) {}

void Lock::acquire() {
    auto c = zdb_generate_random_alphanumeric_string(16);
    client.waitSet(lockKey, Value{c, 0});
}

void Lock::release() {
    auto v = client.waitGet(lockKey, 1);
    client.waitSet(lockKey, v);
    while (true) {
        auto t = client.erase(lockKey);
        if (t.has_value()) {
            return;
        } else {
            if (client.waitNotFound(lockKey)) {
                return;
            } else {
                if (!client.waitGet(lockKey, Value{v.data, 2})) {
                    return;
                }
            }
        }
    }
    std::unreachable();
}

} // namespace zdb
