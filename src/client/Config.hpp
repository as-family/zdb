#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include "client/KVRPCService.hpp"
#include <expected>
#include "common/Error.hpp"

namespace zdb {

class Config {
public:
    using map = std::unordered_map<std::string, KVRPCService>;
    using iterator = map::iterator;
    Config(const std::vector<std::string>& addresses, const RetryPolicy& policy);
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    [[nodiscard]] std::expected<KVRPCService*, Error> currentService();
    std::expected<KVRPCService*, Error> nextService();
    const RetryPolicy policy;  // Store by value, not reference!
private:
    iterator nextActiveServiceIterator();
    map services;
    iterator cService;
};
} // namespace zdb

#endif // CONFIG_H