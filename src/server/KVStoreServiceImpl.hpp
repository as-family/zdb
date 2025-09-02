#ifndef SRC_SERVER_KVSTORESERVICEIMPL_HPP
#define SRC_SERVER_KVSTORESERVICEIMPL_HPP

#include <memory>
#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include "proto/kvStore.grpc.pb.h"
#include "server/RPCServer.hpp"
#include "common/KVStateMachine.hpp"

namespace zdb {

class KVStoreServiceImpl final : public kvStore::KVStoreService::Service {
public:
    KVStoreServiceImpl(KVStateMachine& kv);
    grpc::Status get(
        grpc::ServerContext* context,
        const kvStore::GetRequest* request,
        kvStore::GetReply* reply) override;
    grpc::Status set(
        grpc::ServerContext* context,
        const kvStore::SetRequest* request,
        kvStore::SetReply* reply) override;
    grpc::Status erase(
        grpc::ServerContext* context,
        const kvStore::EraseRequest* request,
        kvStore::EraseReply* reply) override;
    grpc::Status size(
        grpc::ServerContext* context,
        const kvStore::SizeRequest* request,
        kvStore::SizeReply* reply) override;
    ~KVStoreServiceImpl();
private:
    KVStateMachine& kvStateMachine;
};

using KVStoreServer = RPCServer<KVStoreServiceImpl>;

} // namespace zdb

#endif // SRC_SERVER_KVSTORESERVICEIMPL_HPP
