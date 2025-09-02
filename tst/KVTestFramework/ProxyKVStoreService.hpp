#ifndef PROXY_KV_STORE_SERVICE_H
#define PROXY_KV_STORE_SERVICE_H

#include "proto/kvStore.grpc.pb.h"
#include "KVTestFramework/ProxyService.hpp"

class ProxyKVStoreService : public zdb::kvStore::KVStoreService::Service {
public:
    ProxyKVStoreService(ProxyService<zdb::kvStore::KVStoreService>& p);
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
    ProxyService<zdb::kvStore::KVStoreService>& proxy;
};

#endif // PROXY_KV_STORE_SERVICE_H
