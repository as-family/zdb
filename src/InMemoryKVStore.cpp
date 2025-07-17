#include "InMemoryKVStore.hpp"

std::string InMemoryKVStore::get(std::string key) {
    std::shared_lock l {m};
    auto i = store.find(key);
    if (i == store.end()) {
        throw std::out_of_range{""};
    }
    return (*store.find(key)).second;
}

void InMemoryKVStore::set(std::string key, std::string value) {
    std::unique_lock l {m};
    store[key] = value;
}

std::string InMemoryKVStore::erase(std::string key) {
    std::unique_lock l {m};
    auto value = get(key);
    store.erase(key);
    return value;
}
