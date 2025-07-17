#ifndef IN_MEMORY_KV_STORE_H
#define IN_MEMORY_KV_STORE_H

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>
#include <cctype>

class InMemoryKVStore {
public:
    std::string get(std::string key) const;
    void set(std::string key, std::string value);
    std::string erase(std::string key);
    size_t size() const;
private:
    std::unordered_map<std::string, std::string> store;
    mutable std::shared_mutex m;
};

#endif // IN_MEMORY_KV_STORE_H