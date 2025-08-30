#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <expected>
#include "common/Error.hpp"
#include "common/RPCService.hpp"
#include <proto/kvStore.grpc.pb.h>
#include <vector>
#include "common/RetryPolicy.hpp"

namespace zdb {

using KVRPCService = RPCService<zdb::kvStore::KVStoreService>;
using KVRPCServicePtr = RPCService<zdb::kvStore::KVStoreService>*;

class Config {
public:
    using map = std::unordered_map<std::string, KVRPCService>;
    using iterator = map::iterator;
    Config(const std::vector<std::string>& addresses, const RetryPolicy policy);
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    [[nodiscard]] std::expected<KVRPCServicePtr, Error> currentService();
    std::expected<KVRPCServicePtr, Error> nextService();
    std::expected<KVRPCServicePtr, Error> forceNextService();
    void resetUsed();
    const RetryPolicy policy;
private:
    iterator nextActiveServiceIterator();
    map services;
    iterator cService;
    std::unordered_map<std::string, bool> used;
};
} // namespace zdb

#endif // CONFIG_H
