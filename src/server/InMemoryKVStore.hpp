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

namespace zdb {

class InMemoryKVStore {
public:
    std::expected<std::optional<std::string>, Error> get(const std::string key) const;
    std::expected<void, Error> set(const std::string key, const std::string value);
    std::expected<std::optional<std::string>, Error> erase(const std::string key);
    size_t size() const;
private:
    std::unordered_map<std::string, std::string> store;
    mutable std::shared_mutex m;
};

} // namespace zdb

#endif // IN_MEMORY_KV_STORE_H