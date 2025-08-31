#include "ProxyKVStoreService.hpp"
#include <grpcpp/grpcpp.h>

ProxyKVStoreService::ProxyKVStoreService(const std::string& original, NetworkConfig& c)
    : originalAddress{original}, channel{grpc::CreateChannel(original, grpc::InsecureChannelCredentials())},
      stub{zdb::kvStore::KVStoreService::NewStub(channel)}, networkConfig{c} {}

grpc::Status ProxyKVStoreService::get(
    grpc::ServerContext* context,
    const zdb::kvStore::GetRequest* request,
    zdb::kvStore::GetReply* reply) {
    return call(&zdb::kvStore::KVStoreService::Stub::get, request, reply);
}

grpc::Status ProxyKVStoreService::set(
    grpc::ServerContext* context,
    const zdb::kvStore::SetRequest* request,
    zdb::kvStore::SetReply* reply) {
    return call(&zdb::kvStore::KVStoreService::Stub::set, request, reply);
}

grpc::Status ProxyKVStoreService::erase(
    grpc::ServerContext* context,
    const zdb::kvStore::EraseRequest* request,
    zdb::kvStore::EraseReply* reply) {
    return call(&zdb::kvStore::KVStoreService::Stub::erase, request, reply);
}

grpc::Status ProxyKVStoreService::size(
    grpc::ServerContext* context,
    const zdb::kvStore::SizeRequest* request,
    zdb::kvStore::SizeReply* reply) {
    return call(&zdb::kvStore::KVStoreService::Stub::size, request, reply);
}
