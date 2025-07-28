#ifndef KV_STORE_CLIENT_H
#define KV_STORE_CLIENT_H

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "src/proto/kvStore.grpc.pb.h"
#include <expected>
#include <optional>
#include "Error.hpp"

namespace zdb {

class KVStoreClient {
public:
    KVStoreClient(const std::string s_address);
    std::expected<std::optional<std::string>, Error> get(const std::string key) const;
    std::expected<void, Error> set(const std::string key, const std::string value);
    std::expected<std::optional<std::string>, Error> erase(const std::string key);
    std::expected<size_t, Error> size() const;
private:
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<kvStore::KVStoreService::Stub> stub;
};

} // namespace zdb

#endif // KV_STORE_CLIENT_H
