#ifndef KV_STORE_CLIENT_H
#define KV_STORE_CLIENT_H

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "src/proto/KVStore.grpc.pb.h"

class KVStoreClient {
public:
    KVStoreClient(const std::string s_address);
    std::string get(const std::string key) const;
    void set(const std::string key, const std::string value);
    std::string erase(const std::string key);
    size_t size() const;
private:
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<KVStore::KVStoreService::Stub> stub;
};

#endif // KV_STORE_CLIENT_H
