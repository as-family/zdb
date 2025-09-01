#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <unordered_map>
#include <expected>
#include "common/Error.hpp"
#include "common/RPCService.hpp"
#include <proto/kvStore.grpc.pb.h>
#include <proto/kvStore.grpc.pb.h>
#include <random>
#include <vector>
#include "common/RetryPolicy.hpp"
#include <mutex>

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
    std::expected<KVRPCServicePtr, Error> nextService();
    std::expected<KVRPCServicePtr, Error> randomService();
    void resetUsed();
    const RetryPolicy policy;
private:
    iterator nextActiveServiceIterator();
    map services;
    iterator cService;
    std::default_random_engine rng;
    std::uniform_int_distribution<std::size_t> dist;
    std::mutex m;
};
} // namespace zdb

#endif // CONFIG_H
