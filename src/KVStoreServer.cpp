#include "KVStoreServer.hpp"

KVStoreServiceImpl::KVStoreServiceImpl(InMemoryKVStore& kv)
    : kvStore {kv} {}

grpc::Status KVStoreServiceImpl::get(
    grpc::ServerContext *context,
    const KVStore::GetRequest *request,
    KVStore::GetReply *reply) {
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
    const KVStore::SetRquest* request,
    KVStore::SetReply* reply) {
    kvStore.set(request->key(), request->value());
    return grpc::Status::OK;
}

grpc::Status KVStoreServiceImpl::erase(
    grpc::ServerContext* context,
    const KVStore::EraseRequest* request,
    KVStore::EraseReply* reply) {
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
