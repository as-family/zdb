#ifndef IN_MEMORY_KV_STORE_H
#define IN_MEMORY_KV_STORE_H

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>

class InMemoryKVStore {
public:
    std::string get(std::string key);
    void set(std::string key, std::string value);
    std::string erase(std::string key);
private:
    std::unordered_map<std::string, std::string> store;
    std::shared_mutex m;
};

#endif // IN_MEMORY_KV_STORE_H