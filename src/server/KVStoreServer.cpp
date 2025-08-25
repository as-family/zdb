#include "server/KVStoreServer.hpp"
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"
#include "server/InMemoryKVStore.hpp"
#include <grpcpp/support/status.h>
#include <grpcpp/grpcpp.h>
#include "proto/kvStore.pb.h"
#include <tuple>
#include <grpcpp/security/credentials.h>
#include "raft/Command.hpp"
#include "common/Command.hpp"
#include "common/Types.hpp"
#include <variant>

namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(InMemoryKVStore& kv, raft::Raft* r, raft::Channel* c)
    : kvStore {kv},
      raft {r},
      channel {c},
      pendingCommands {} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {
    std::ignore = context;
    Key key{request->key().data()};
    auto g = new Get(key.data);
    if (!raft->start(g)) {
        delete g;
        return toGrpcStatus(Error {ErrorCode::NotLeader});
    } else {
        raft::Command* cmd = channel->receive();
        raft::State* state;
        if (cmd) {
            state = applyCommand(cmd);
        } else {
            return toGrpcStatus(Error {ErrorCode::Internal, "failed to apply command"});
        }
        auto s = static_cast<zdb::State*>(state);
        if (!s) {
            return toGrpcStatus(Error {ErrorCode::Internal, "failed to cast state"});
        }
        const auto& v = std::get<std::expected<std::optional<Value>, Error>>(s->u);
        if (!v.has_value()) {
            return toGrpcStatus(v.error());
        }
        else if (!v->has_value()) {
            return toGrpcStatus(Error {ErrorCode::KeyNotFound, "key not found"});
        } else {
            reply->mutable_value()->set_data(v->value().data);
            reply->mutable_value()->set_version(v->value().version);
            return grpc::Status::OK;
        }
    }
}

raft::State* KVStoreServiceImpl::handleGet(Key key) {
    auto v = kvStore.get(key);
    return new zdb::State {key, v};
}

grpc::Status KVStoreServiceImpl::set(
    grpc::ServerContext *context,
    const kvStore::SetRequest* request,
    kvStore::SetReply *reply) {
    std::ignore = context;
    std::ignore = reply;
    Key key{request->key().data()};
    Value value{request->value().data(), request->value().version()};
    auto g = new Set(key, value);
    if (!raft->start(g)) {
        delete g;
        return toGrpcStatus(Error {ErrorCode::NotLeader});
    } else {
        raft::Command* cmd = channel->receive();
        raft::State* state;
        if (cmd) {
            state = applyCommand(cmd);
        } else {
            return toGrpcStatus(Error {ErrorCode::Internal, "failed to apply command"});
        }
        auto s = static_cast<zdb::State*>(state);
        if (!s) {
            return toGrpcStatus(Error {ErrorCode::Internal, "failed to cast state"});
        }
        auto v = std::get<std::expected<void, Error>>(s->u);
        return toGrpcStatus(v);
    }
}

raft::State* KVStoreServiceImpl::handleSet(Key key, Value value) {
    auto v = kvStore.set(key, value);
    return new zdb::State {key, v};
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    std::ignore = context;
    Key key{request->key().data()};
    auto v = kvStore.erase(key);
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    else if (!v->has_value()) {
        return toGrpcStatus(Error {ErrorCode::KeyNotFound});
    } else {
        reply->mutable_value()->set_data(v->value().data);
        reply->mutable_value()->set_version(v->value().version);
        return grpc::Status::OK;
    }
}

grpc::Status KVStoreServiceImpl::size(
    grpc::ServerContext *context,
    const kvStore::SizeRequest *request,
    kvStore::SizeReply *reply) {
    std::ignore = context;
    std::ignore = request;
    reply->set_size(kvStore.size());
    return grpc::Status::OK;
}

void KVStoreServiceImpl::snapshot() {
    // Implement snapshot logic if needed

}

void KVStoreServiceImpl::restore(const std::string& snapshot) {
    // Implement restore logic if needed
}

State* KVStoreServiceImpl::applyCommand(raft::Command* command) {
    command->apply(this);
}

void KVStoreServiceImpl::consumeChannel() {
    if (!channel) return;
    while (true) {
        raft::Command* cmd = channel->receive();
        if (cmd) {
            applyCommand(cmd);
        } else {
            break;
        }
    }
}

KVStoreServiceImpl::~KVStoreServiceImpl() {
    channel->send(nullptr);
    if (t.joinable()) {
        t.join();
    }
}

} // namespace zdb
