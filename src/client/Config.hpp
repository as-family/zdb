#ifndef CONFIG_H
#define CONFIG_H

#include <unordered_map>
#include <expected>
#include "common/Error.hpp"
#include "common/RPCService.hpp"
#include <proto/kvStore.grpc.pb.h>
#include <proto/kvStore.pb.h>
#include <random>
#include <vector>
#include "common/RetryPolicy.hpp"
#include <mutex>
#include <string>
#include <functional>

namespace zdb {

using KVRPCService = RPCService<zdb::kvStore::KVStoreService>;
using KVRPCServicePtr = RPCService<zdb::kvStore::KVStoreService>*;

inline std::unordered_map<std::string, KVRPCService::function_t> getDefaultKVFunctions() {
    return {
        { "get", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->get(ctx, static_cast<const zdb::kvStore::GetRequest&>(req), static_cast<zdb::kvStore::GetReply*>(resp));
        }},
        { "set", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->set(ctx, static_cast<const zdb::kvStore::SetRequest&>(req), static_cast<zdb::kvStore::SetReply*>(resp));
        }},
        { "erase", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->erase(ctx, static_cast<const zdb::kvStore::EraseRequest&>(req), static_cast<zdb::kvStore::EraseReply*>(resp));
        }},
        { "size", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->size(ctx, static_cast<const zdb::kvStore::SizeRequest&>(req), static_cast<zdb::kvStore::SizeReply*>(resp));
        }}
    };
}

class Config {
public:
    using map = std::unordered_map<std::string, KVRPCService>;
    using iterator = map::iterator;
    Config(const std::vector<std::string>& addresses, const RetryPolicy policy, std::unordered_map<std::string, KVRPCService::function_t> f = getDefaultKVFunctions());
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
