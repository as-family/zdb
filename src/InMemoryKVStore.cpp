#include "InMemoryKVStore.hpp"

std::string InMemoryKVStore::get(const std::string key) const {
    std::shared_lock l {m};
    auto i = store.find(key);
    if (i == store.end()) {
        throw std::out_of_range{""};
    }
    return (*store.find(key)).second;
}

void InMemoryKVStore::set(const std::string key, const std::string value) {
    std::unique_lock l {m};
    store[key] = value;
}

std::string InMemoryKVStore::erase(const std::string key) {
    std::unique_lock l {m};
    auto value = get(key);
    store.erase(key);
    return value;
}

size_t InMemoryKVStore::size() const {
    std::shared_lock l {m};
    return store.size();
}
