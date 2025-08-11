#ifndef IN_MEMORY_KV_STORE_H
#define IN_MEMORY_KV_STORE_H

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <cctype>
#include <expected>
#include <optional>
#include "common/Error.hpp"
#include "interface/StorageEngine.hpp"

namespace zdb {

class InMemoryKVStore : public StorageEngine {
public:
    std::expected<std::optional<Value>, Error> get(const Key& key) const override;
    std::expected<void, Error> set(const Key& key, const Value& value) override;
    std::expected<std::optional<Value>, Error> erase(const Key& key) override;
    size_t size() const;
private:
    std::unordered_map<Key, Value, KeyHash> store;
    mutable std::shared_mutex m;
};

} // namespace zdb

#endif // IN_MEMORY_KV_STORE_H
