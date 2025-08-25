#ifndef PROXY_KV_STORE_SERVICE_H
#define PROXY_KV_STORE_SERVICE_H

#include "proto/kvStore.grpc.pb.h"
#include "NetworkConfig.hpp"
#include <thread>
#include <spdlog/spdlog.h>

class ProxyKVStoreService : public zdb::kvStore::KVStoreService::Service {
public:
    ProxyKVStoreService(const std::string original, NetworkConfig& c);
    template<typename Req, typename Rep>
    grpc::Status call(
        grpc::Status (zdb::kvStore::KVStoreService::Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req* request,
        Rep* reply) const {
        grpc::ClientContext c;
        if (networkConfig.reliable()) {
            std::cerr << "network" << std::endl;
            auto status = (stub.get()->*f)(&c, *request, reply);
            return status;
        } else {
            std::cerr << "Unreliable network" << std::endl;
            if (networkConfig.drop()) {
                return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped");
            }
            auto status = (stub.get()->*f)(&c, *request, reply);
            if (networkConfig.drop()) {
               return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped");
            }
            if (networkConfig.delay()) {
                std::this_thread::sleep_for(networkConfig.delayTime());
            }
            return status;
        }
    }
    grpc::Status get(
        grpc::ServerContext* context,
        const zdb::kvStore::GetRequest* request,
        zdb::kvStore::GetReply* reply) override;
    grpc::Status set(
        grpc::ServerContext* context,
        const zdb::kvStore::SetRequest* request,
        zdb::kvStore::SetReply* reply) override;
    grpc::Status erase(
        grpc::ServerContext* context,
        const zdb::kvStore::EraseRequest* request,
        zdb::kvStore::EraseReply* reply) override;
    grpc::Status size(
        grpc::ServerContext* context,
        const zdb::kvStore::SizeRequest* request,
        zdb::kvStore::SizeReply* reply) override;
private:
    std::string originalAddress;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<zdb::kvStore::KVStoreService::Stub> stub;
    NetworkConfig& networkConfig;
};

#endif // PROXY_KV_STORE_SERVICE_H
