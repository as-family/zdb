#include "KVStoreServer.hpp"

namespace zdb {

KVStoreServiceImpl::KVStoreServiceImpl(InMemoryKVStore& kv)
    : kvStore {kv} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const kvStore::GetRequest *request,
    kvStore::GetReply *reply) {
    try {
        auto value = kvStore.get(request->key());
        reply->set_value(value);
        return grpc::Status::OK;
    } catch(std::out_of_range& e) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "key not found");
    }
}

grpc::Status KVStoreServiceImpl::set(
    grpc::ServerContext* context,
    const kvStore::SetRquest* request,
    kvStore::SetReply* reply) {
    kvStore.set(request->key(), request->value());
    return grpc::Status::OK;
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const kvStore::EraseRequest* request,
    kvStore::EraseReply* reply) {
    try {
        reply->set_value(kvStore.erase(request->key()));
        return grpc::Status::OK;
    } catch(std::out_of_range& e) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "key not found");
    }
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

} // namespace zdb
