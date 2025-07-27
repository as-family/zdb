#ifndef IN_MEMORY_KV_STORE_H
#define IN_MEMORY_KV_STORE_H

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>
#include <cctype>

namespace zdb {

class InMemoryKVStore {
public:
    std::string get(const std::string key) const;
    void set(const std::string key, const std::string value);
    std::string erase(const std::string key);
    size_t size() const;
private:
    std::unordered_map<std::string, std::string> store;
    mutable std::shared_mutex m;
};

} // namespace zdb

#endif // IN_MEMORY_KV_STORE_H