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
    Config(const std::vector<std::string>& addresses, const RetryPolicy& Policy);
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    std::expected<KVRPCService*, Error> currentService() const;
    std::expected<KVRPCService*, Error> nextService();
    const RetryPolicy& Policy;
private:
    iterator nextActiveServiceIterator();
    map services;
    iterator cService;
};
}

#endif // CONFIG_H