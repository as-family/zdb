#include "server/KVStoreServiceImpl.hpp"
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"
#include <grpcpp/support/status.h>
#include "proto/kvStore.pb.h"
#include "raft/Command.hpp"
#include "raft/Raft.hpp"
#include "common/Command.hpp"
#include "common/Types.hpp"
#include <variant>
#include <tuple>

namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(KVStateMachine* kv)
    : kvStateMachine {kv} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {
    std::ignore = context;
    Key key{request->key().data()};
    auto state = kvStateMachine->handleGet(key);
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

grpc::Status KVStoreServiceImpl::set(
    grpc::ServerContext *context,
    const kvStore::SetRequest* request,
    kvStore::SetReply *reply) {
    std::ignore = context;
    std::ignore = reply;
    Key key{request->key().data()};
    Value value{request->value().data(), request->value().version()};
    auto state = kvStateMachine->handleSet(key, value);
    auto s = static_cast<zdb::State*>(state);
    if (!s) {
        return toGrpcStatus(Error {ErrorCode::Internal, "failed to cast state"});
    }
    auto v = std::get<std::expected<std::monostate, Error>>(s->u);
    return toGrpcStatus(v);
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    std::ignore = context;
    Key key{request->key().data()};
    auto state = kvStateMachine->handleErase(key);
    auto s = static_cast<zdb::State*>(state);
    if (!s) {
        return toGrpcStatus(Error {ErrorCode::Internal, "failed to cast state"});
    }
    auto v = std::get<std::expected<std::optional<Value>, Error>>(s->u);
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

grpc::Status KVStoreServiceImpl::size(
    grpc::ServerContext *context,
    const kvStore::SizeRequest *request,
    kvStore::SizeReply *reply) {
    std::ignore = context;
    std::ignore = request;
    auto state = kvStateMachine->handleSize();
    auto s = static_cast<zdb::State*>(state);
    if (!s) {
        return toGrpcStatus(Error {ErrorCode::Internal, "failed to cast state"});
    }
    auto v = std::get<std::expected<size_t, Error>>(s->u);
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    reply->set_size(v.value());
    return grpc::Status::OK;
}

KVStoreServiceImpl::~KVStoreServiceImpl() {
    delete kvStateMachine;
}

} // namespace zdb
