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

InMemoryKVStore::InMemoryKVStore() : store{}, m{} {}

std::expected<std::optional<Value>, Error> InMemoryKVStore::get(const Key& key) const {
    std::unique_lock lock {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    return i->second;
}

std::expected<void, Error> InMemoryKVStore::set(const Key& key, const Value& value) {
    std::unique_lock lock {m};
    auto i = store.find(key);
    if (i != store.end()) {
        if (value.version != i->second.version) {
            // std::cerr << key.data << " Version mismatch: expected " << i->second.version << " but got " << value.version << "\n";
            return std::unexpected {Error {ErrorCode::VersionMismatch, "Version mismatch: expected " + std::to_string(i->second.version) + " but got " + std::to_string(value.version), key.data, i->second.data, i->second.version}};
        }
        // std::cerr << key.data << " Updating value to: " << value.data << "\n";
        i->second = Value{value.data, i->second.version + 1};
    } else {
        if (value.version != 0) {
            // std::cerr << key.data << " Key does not exist, must use version 0 for new keys\n";
            return std::unexpected {Error {ErrorCode::KeyNotFound, "Key does not exist, must use version 0 for new keys", key.data, value.data, 0}};
        }
        // std::cerr << key.data << " Adding new key with value: " << value.data << "\n";
        store.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(value.data, 1));
    }
    return {};
}

std::expected<std::optional<Value>, Error> InMemoryKVStore::erase(const Key& key) {
    std::unique_lock lock {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    auto v = i->second;
    store.erase(i);
    return v;
}

size_t InMemoryKVStore::size() const {
    std::unique_lock lock {m};
    return store.size();
}

} // namespace zdb
