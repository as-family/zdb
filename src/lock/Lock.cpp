#include "lock/Lock.hpp"
#include "common/Types.hpp"
#include <iostream>

namespace zdb {

Lock::Lock(Key key, KVStoreClient& c) : lock_key(key), client(c) {}

bool Lock::acquire() {
    auto v = client.set(lock_key, Value{"locked_value", 0});
    return v.has_value();
}

bool Lock::release() {
    auto v = client.erase(lock_key);
    return v.has_value() || v.error().code == ErrorCode::KeyNotFound;
}

} // namespace zdb
