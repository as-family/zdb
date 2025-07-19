#ifndef SIMPLE_H
#define SIMPLE_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include "src/proto/KVStore.grpc.pb.h"

class Simple final : public KVStore::KVStoreService::Service {
public:
    Simple();
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
};

class Server {
public:
    Server(std::string, Simple*);
    void wait();
    ~Server();
private:
    std::string listen_address;
    Simple* service;
    std::unique_ptr<grpc::Server> server;
};

#endif // SIMPLE_H
