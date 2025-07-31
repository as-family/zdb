#ifndef KV_STORE_CLIENT_H
#define KV_STORE_CLIENT_H

#include <string>
#include <memory>
#include <expected>
#include <optional>
#include <vector>
#include <unordered_map>
#include "Error.hpp"
#include "KVRPCService.hpp"
#include "RetryPolicy.hpp"

namespace zdb {

class KVStoreClient {
public:
    KVStoreClient(const std::vector<std::string>& addresses, RetryPolicy& r);
    std::expected<std::optional<std::string>, Error> get(const std::string key) const;
    std::expected<void, Error> set(const std::string key, const std::string value);
    std::expected<std::optional<std::string>, Error> erase(const std::string key);
    std::expected<size_t, Error> size() const;
private:
    std::unordered_map<std::string, KVRPCService> services;
    std::unordered_map<std::string, KVRPCService>::iterator currentService;
};

} // namespace zdb

#endif // KV_STORE_CLIENT_H
