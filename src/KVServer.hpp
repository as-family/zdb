#ifndef KV_SERVER_H
#define KV_SERVER_H

#include "InMemoryKVStore.hpp"
#include <memory>
#include <grpcpp/grpcpp.h>
#include "src/proto/KVStore.grpc.pb.h"

class KVServer {
public:
    KVServer(std::string l_address, InMemoryKVStore& kvs);
    void wait();
    ~KVServer();
protected:
    InMemoryKVStore& kv();
private:
    class KVServerImpl final : public KVStore::KVStoreService::Service {
    public:
        KVServerImpl(KVServer* kvs);
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
        KVServer* kvServer;
    };
    std::string listen_address;
    InMemoryKVStore& kvStore;
    KVServerImpl service;
    std::unique_ptr<grpc::Server> server;
};

#endif // KV_SERVER_H
