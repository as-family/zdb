#include "ProxyKVStoreService.hpp"
#include <grpcpp/grpcpp.h>
#include "common/ErrorConverter.hpp"

ProxyKVStoreService::ProxyKVStoreService(ProxyService<zdb::kvStore::KVStoreService>& p)
    : proxy{p} {}

grpc::Status ProxyKVStoreService::get(
    grpc::ServerContext* context,
    const zdb::kvStore::GetRequest* request,
    zdb::kvStore::GetReply* reply) {
    auto t = proxy.call("get", *request, *reply);
    if (t.has_value()) {
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}

grpc::Status ProxyKVStoreService::set(
    grpc::ServerContext* context,
    const zdb::kvStore::SetRequest* request,
    zdb::kvStore::SetReply* reply) {
    auto t = proxy.call("set", *request, *reply);
    if (t.has_value()) {
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}

grpc::Status ProxyKVStoreService::erase(
    grpc::ServerContext* context,
    const zdb::kvStore::EraseRequest* request,
    zdb::kvStore::EraseReply* reply) {
    auto t = proxy.call("erase", *request, *reply);
    if (t.has_value()) {
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}

grpc::Status ProxyKVStoreService::size(
    grpc::ServerContext* context,
    const zdb::kvStore::SizeRequest* request,
    zdb::kvStore::SizeReply* reply) {
    auto t = proxy.call("size", *request, *reply);
    if (t.has_value()) {
        return grpc::Status::OK;
    }
    return zdb::toGrpcStatus(t.error().back());
}
