#include "InMemoryKVStore.hpp"
#include <expected>
#include <optional>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <cstddef>
#include "common/Error.hpp"

namespace zdb {

std::expected<std::optional<std::string>, Error> InMemoryKVStore::get(const std::string& key) const {
    const std::shared_lock LOCK {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    return i->second;
}

std::expected<void, Error> InMemoryKVStore::set(const std::string& key, const std::string& value) {
    const std::unique_lock LOCK {m};
    store[key] = value;
    return {};
}

std::expected<std::optional<std::string>, Error> InMemoryKVStore::erase(const std::string& key) {
    const std::unique_lock LOCK {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    auto v = i->second;
    store.erase(i);
    return v;
}

size_t InMemoryKVStore::size() const {
    const std::shared_lock LOCK {m};
    return store.size();
}

} // namespace zdb
