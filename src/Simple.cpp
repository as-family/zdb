#include "Simple.hpp"

Simple::Simple() {}

grpc::Status Simple::get(
    grpc::ServerContext *context,
    const KVStore::GetRequest *request,
    KVStore::GetReply *reply) {
    try {
        // auto value = kvServer->kv().get(request->key());
        reply->set_value("get");
        return grpc::Status::OK;
    } catch(std::out_of_range& e) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "key not found");
    }
}

grpc::Status Simple::set(
    grpc::ServerContext* context,
    const KVStore::SetRquest* request,
    KVStore::SetReply* reply) {
    // kvServer->kv().set(request->key(), request->value());
    return grpc::Status::OK;
}

grpc::Status Simple::erase(
    grpc::ServerContext* context,
    const KVStore::EraseRequest* request,
    KVStore::EraseReply* reply) {
    try {
        // auto value = kvServer->kv().erase(request->key());
        reply->set_value("erase");
        return grpc::Status::OK;
    } catch(std::out_of_range& e) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "key not found");
    }
}

Server::Server(std::string l_address, Simple* s)
    : listen_address{l_address}, service {s} {
    grpc::ServerBuilder sb{};
    sb.AddListeningPort(listen_address, grpc::InsecureServerCredentials());
    sb.RegisterService(service);
    server = sb.BuildAndStart();
}

void Server::wait() {
    server->Wait();
}

Server::~Server() {
    server->Shutdown();
}
