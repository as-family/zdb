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
        { "get", [](zdb::kvStore::KVStoreService::Stub* stub,
                    grpc::ClientContext* ctx,
                    const google::protobuf::Message& req,
                    google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::GetRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "get: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::GetRequest&>(req);
            auto* p = static_cast<zdb::kvStore::GetReply*>(resp);
            return stub->get(ctx, r, p);
        }},
        { "set", [](zdb::kvStore::KVStoreService::Stub* stub,
                    grpc::ClientContext* ctx,
                    const google::protobuf::Message& req,
                    google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::SetRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "set: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::SetRequest&>(req);
            auto* p = static_cast<zdb::kvStore::SetReply*>(resp);
            return stub->set(ctx, r, p);
        }},
        { "erase", [](zdb::kvStore::KVStoreService::Stub* stub,
                      grpc::ClientContext* ctx,
                      const google::protobuf::Message& req,
                      google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::EraseRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "erase: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::EraseRequest&>(req);
            auto* p = static_cast<zdb::kvStore::EraseReply*>(resp);
            return stub->erase(ctx, r, p);
        }},
        { "size", [](zdb::kvStore::KVStoreService::Stub* stub,
                     grpc::ClientContext* ctx,
                     const google::protobuf::Message& req,
                     google::protobuf::Message* resp) -> grpc::Status {
            if (!resp ||
                req.GetDescriptor() != zdb::kvStore::SizeRequest::descriptor()) {
                return {grpc::StatusCode::INVALID_ARGUMENT,
                        "size: type mismatch or null resp"};
            }
            auto& r = static_cast<const zdb::kvStore::SizeRequest&>(req);
            auto* p = static_cast<zdb::kvStore::SizeReply*>(resp);
            return stub->size(ctx, r, p);
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
