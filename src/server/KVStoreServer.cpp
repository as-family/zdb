#include "server/KVStoreServer.hpp"
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"
#include "server/InMemoryKVStore.hpp"
#include <grpcpp/support/status.h>
#include <grpcpp/grpcpp.h>
#include "proto/kvStore.pb.h"
#include <tuple>
#include <grpcpp/security/credentials.h>


namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(InMemoryKVStore& kv)
    : kvStore {kv} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {
    std::ignore = context;
    // Convert protobuf Key to C++ Key
    Key key{request->key().data()};
    auto v = kvStore.get(key);
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    else if (!v->has_value()) {
        return toGrpcStatus(Error {ErrorCode::NotFound, "key not found"});
    } else {
        // Convert C++ Value to protobuf Value
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
    // Convert protobuf types to C++ types
    Key key{request->key().data()};
    Value value{request->value().data(), request->value().version()};
    auto v = kvStore.set(key, value);
    return toGrpcStatus(v);
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    std::ignore = context;
    // Convert protobuf Key to C++ Key
    Key key{request->key().data()};
    auto v = kvStore.erase(key);
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    else if (!v->has_value()) {
        return toGrpcStatus(Error {ErrorCode::NotFound, "key not found"});
    } else {
        // Convert C++ Value to protobuf Value
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

KVStoreServer::KVStoreServer(const std::string& address, KVStoreServiceImpl& s)
    : addr{address}, service {s} {
    grpc::ServerBuilder sb{};
    sb.AddListeningPort(addr, grpc::InsecureServerCredentials());
    sb.RegisterService(&service);
    server = sb.BuildAndStart();
}

void KVStoreServer::wait() {
    server->Wait();
}

void KVStoreServer::shutdown() {
    if (server) {
        server->Shutdown();
        server.reset();
    }
}

} // namespace zdb
