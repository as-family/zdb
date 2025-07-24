#ifndef KV_STORE_SERVER_H
#define KV_STORE_SERVER_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include "src/proto/KVStore.grpc.pb.h"
#include "InMemoryKVStore.hpp"

class KVStoreServiceImpl final : public KVStore::KVStoreService::Service {
public:
    KVStoreServiceImpl(InMemoryKVStore&);
    grpc::Status get(
        grpc::ServerContext* context,
        const KVStore::GetRequest* request,
        KVStore::GetReply* reply) override;
    grpc::Status set(
        grpc::ServerContext* context,
        const KVStore::SetRquest* request,
        KVStore::SetReply* reply) override;
    grpc::Status erase(
        grpc::ServerContext* context,
        const KVStore::EraseRequest* request,
        KVStore::EraseReply* reply) override;
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

#endif // KV_STORE_SERVER_H
