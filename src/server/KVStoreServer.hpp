#ifndef KV_STORE_SERVER_H
#define KV_STORE_SERVER_H

#include <memory>
#include <grpcpp/grpcpp.h>
#include "proto/kvStore.grpc.pb.h"
#include "InMemoryKVStore.hpp"
#include "server/RPCServer.hpp"
#include "raft/StateMachine.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include <thread>

namespace zdb {

class KVStoreServiceImpl final : public kvStore::KVStoreService::Service, public raft::StateMachine {
public:
    KVStoreServiceImpl(InMemoryKVStore& kv, raft::Raft* r, raft::Channel* c);
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
    raft::State* handleGet(Key key);
    raft::State* handleSet(Key key, Value value);
    void snapshot() override;
    void restore(const std::string& snapshot) override;
    raft::State* applyCommand(raft::Command* command) override;
    void consumeChannel() override;
    ~KVStoreServiceImpl();
private:
    InMemoryKVStore& kvStore;
    raft::Raft* raft;
    raft::Channel* channel;
    std::thread t;
    std::unordered_map<raft::Command*, std::function<grpc::Status()>> pendingCommands;
};

using KVStoreServer = RPCServer<KVStoreServiceImpl>;

} // namespace zdb

#endif // KV_STORE_SERVER_H
