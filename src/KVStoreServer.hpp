#ifndef KV_STORE_SERVER_H
#define KV_STORE_SERVER_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include "src/proto/kvStore.grpc.pb.h"
#include "InMemoryKVStore.hpp"

namespace zdb {

class KVStoreServiceImpl final : public kvStore::KVStoreService::Service {
public:
    KVStoreServiceImpl(InMemoryKVStore&);
    grpc::Status get(
        grpc::ServerContext* context,
        const kvStore::GetRequest* request,
        kvStore::GetReply* reply) override;
    grpc::Status set(
        grpc::ServerContext* context,
        const kvStore::SetRquest* request,
        kvStore::SetReply* reply) override;
    grpc::Status erase(
        grpc::ServerContext* context,
        const kvStore::EraseRequest* request,
        kvStore::EraseReply* reply) override;
private:
    InMemoryKVStore& kvStore;
};

class KVStoreServer {
public:
    KVStoreServer(const std::string, KVStoreServiceImpl&);
    void wait();
private:
    std::string listen_address;
    KVStoreServiceImpl& service;
    std::unique_ptr<grpc::Server> server;
};

} // namespace zdb

#endif // KV_STORE_SERVER_H
