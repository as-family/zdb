#include "InMemoryKVStore.hpp"

namespace zdb {

std::expected<std::optional<std::string>, Error> InMemoryKVStore::get(const std::string key) const {
    std::shared_lock l {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    return i->second;
}

std::expected<void, Error> InMemoryKVStore::set(const std::string key, const std::string value) {
    std::unique_lock l {m};
    store[key] = value;
    return {};
}

std::expected<std::optional<std::string>, Error> InMemoryKVStore::erase(const std::string key) {
    std::unique_lock l {m};
    auto i = store.find(key);
    if (i == store.end()) {
        return std::nullopt;
    }
    auto v = i->second;
    store.erase(i);
    return v;
}

size_t InMemoryKVStore::size() const {
    std::shared_lock l {m};
    return store.size();
}

} // namespace zdb
