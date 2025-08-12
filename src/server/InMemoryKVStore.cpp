#include "InMemoryKVStore.hpp"
#include <optional>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <cstddef>
#include "common/Error.hpp"
#include "common/Types.hpp"
#include <expected>
#include <optional>

namespace zdb {

std::expected<std::optional<Value>, Error> InMemoryKVStore::get(const Key& key) const {
    const std::shared_lock lock {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    return i->second;
}

std::expected<void, Error> InMemoryKVStore::set(const Key& key, const Value& value) {
    const std::unique_lock lock {m};
    auto i = store.find(key);
    if (i != store.end()) {
        if (value.version != i->second.version) {
            return std::unexpected {Error {ErrorCode::VersionMismatch, "Version mismatch: expected " + std::to_string(i->second.version) + " but got " + std::to_string(value.version)}};
        }
        i->second = Value{value.data, i->second.version + 1};
    } else {
        if (value.version != 0) {
            return std::unexpected {Error {ErrorCode::KeyNotFound, "Key does not exist, must use version 0 for new keys"}};
        }
        store[key] = Value{value.data, 1};
    }
    return {};
}

std::expected<std::optional<Value>, Error> InMemoryKVStore::erase(const Key& key) {
    const std::unique_lock lock {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    auto v = i->second;
    store.erase(i);
    return v;
}

size_t InMemoryKVStore::size() const {
    const std::shared_lock lock {m};
    return store.size();
}

} // namespace zdb
