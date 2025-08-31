#include "lock/Lock.hpp"
#include "common/Types.hpp"
#include <random>
#include <string>
#include <algorithm>
#include "common/Util.hpp"

namespace zdb {

Lock::Lock(const Key key, KVStoreClient& c) : lockKey(key), lockValue {"", 1000}, client(c) {}

void Lock::acquire() {
    auto c = zdb_generate_random_alphanumeric_string(16);
    client.waitSet(lockKey, Value{c, 0});
    lockValue = Value{c, 1};
}

void Lock::release() {
    auto v = client.waitGet(lockKey, 1);
    if (v != lockValue) {
        return;
    }
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
