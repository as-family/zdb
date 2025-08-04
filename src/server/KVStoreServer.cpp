#include "KVStoreServer.hpp"
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"

namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(InMemoryKVStore& kv)
    : kvStore {kv} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {

    auto v = kvStore.get(request->key());
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    else if (!v->has_value()) {
        return toGrpcStatus(Error {ErrorCode::NotFound, "key not found"});
    } else {
        reply->set_value(v->value());
        return grpc::Status::OK;
    }
}

grpc::Status KVStoreServiceImpl::set(
    grpc::ServerContext* context,
    const kvStore::SetRequest* request,
    kvStore::SetReply* reply) {
    auto v = kvStore.set(request->key(), request->value());
    return toGrpcStatus(std::move(v));
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    auto v = kvStore.erase(request->key());
    if (!v.has_value()) {
        return toGrpcStatus(v.error());
    }
    else if (!v->has_value()) {
        return toGrpcStatus(Error {ErrorCode::NotFound, "key not found"});
    } else {
        reply->set_value(v->value());
        return grpc::Status::OK;
    }
}

grpc::Status KVStoreServiceImpl::size(
    grpc::ServerContext* context,
    const kvStore::SizeRequest* request,
    kvStore::SizeReply* reply) {
    reply->set_size(kvStore.size());
    return grpc::Status::OK;
}

KVStoreServer::KVStoreServer(const std::string l_address, KVStoreServiceImpl& s)
    : listen_address{l_address}, service {s} {
    grpc::ServerBuilder sb{};
    sb.AddListeningPort(listen_address, grpc::InsecureServerCredentials());
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
