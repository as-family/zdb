#include "KVServer.hpp"

KVServer::KVServer(std::string l_address, InMemoryKVStore& kvs)
: listen_address{l_address}, kvStore{kvs}, service {KVServerImpl {this}} {
    grpc::ServerBuilder sb{};
    sb.AddListeningPort(listen_address, grpc::InsecureServerCredentials());
    sb.RegisterService(&service);
    server = sb.BuildAndStart();
}

InMemoryKVStore& KVServer::kv() {
    return kvStore;
}

void KVServer::wait() {
    server.get()->Wait();
}

KVServer::~KVServer() {
    server.get()->Shutdown();
}

KVServer::KVServerImpl::KVServerImpl(KVServer *kvs) : kvServer{kvs} {}

grpc::Status KVServer::KVServerImpl::get(
    grpc::ServerContext *context,
    const KVStore::GetRequest *request,
    KVStore::GetReply *reply) {
    try {
        auto value = kvServer->kv().get(request->key());
        reply->set_value(value);
        return grpc::Status::OK;
    } catch(std::out_of_range& e) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "key not found");
    }
}

grpc::Status KVServer::KVServerImpl::set(
    grpc::ServerContext* context,
    const KVStore::SetRquest* request,
    KVStore::SetReply* reply) {
    kvServer->kv().set(request->key(), request->value());
    return grpc::Status::OK;
}

grpc::Status KVServer::KVServerImpl::erase(
    grpc::ServerContext* context,
    const KVStore::EraseRequest* request,
    KVStore::EraseReply* reply) {
    try {
        auto value = kvServer->kv().erase(request->key());
        reply->set_value(value);
        return grpc::Status::OK;
    } catch(std::out_of_range& e) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "key not found");
    }
}
