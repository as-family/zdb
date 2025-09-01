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
#include "common/Util.hpp"

namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(KVStateMachine& kv)
    : kvStateMachine {kv} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {
    std::ignore = context;
    Key key{request->key().data()};
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    auto g = Get{uuid, key};
    auto state = static_cast<State*>(kvStateMachine.handleGet(g, context->deadline()).get());
    const auto& v = std::get<std::expected<std::optional<Value>, Error>>(state->u);
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
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    auto s = Set{uuid, key, value};
    auto state = static_cast<State*>(kvStateMachine.handleSet(s, context->deadline()).get());
    auto v = std::get<std::expected<std::monostate, Error>>(state->u);
    return toGrpcStatus(v);
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    std::ignore = context;
    Key key{request->key().data()};
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    auto e = Erase{uuid, key};
    auto state = static_cast<State*>(kvStateMachine.handleErase(e, context->deadline()).get());
    auto v = std::get<std::expected<std::optional<Value>, Error>>(state->u);
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
    auto uuid = string_to_uuid_v7(request->requestid().uuid());
    auto sz = Size{uuid};
    auto state = static_cast<State*>(kvStateMachine.handleSize(sz, context->deadline()).get());
    auto v = std::get<std::expected<size_t, Error>>(state->u);
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    reply->set_size(v.value());
    return grpc::Status::OK;
}

KVStoreServiceImpl::~KVStoreServiceImpl() {
}

} // namespace zdb
