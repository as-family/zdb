#include "KVStoreClient.hpp"

KVStoreClient::KVStoreClient(const std::string s_address)
    : channel {grpc::CreateChannel(s_address, grpc::InsecureChannelCredentials())},
      stub {KVStore::KVStoreService::NewStub(channel)} {}
    
std::string KVStoreClient::get(const std::string key) const {
    KVStore::GetRequest request;
    request.set_key(key);
    KVStore::GetReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->get(&context, request, &reply)};
    if (status.ok()) {
        return reply.value();
    } else {
        throw status;
    }
}

void KVStoreClient::set(const std::string key, const std::string value) {
    KVStore::SetRquest request;
    request.set_key(key);
    request.set_value(value);
    KVStore::SetReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->set(&context, request, &reply)};
    if (!status.ok()) {
        throw status;
    }
}

std::string KVStoreClient::erase(const std::string key) {
    KVStore::EraseRequest request;
    request.set_key(key);
    KVStore::EraseReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->erase(&context, request, &reply)};
    if (status.ok()) {
        return reply.value();
    } else {
        throw status;
    }
}

size_t KVStoreClient::size() const {
    KVStore::SizeRequest request;
    KVStore::SizeReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->size(&context, request, &reply)};
    if (status.ok()) {
        return reply.size();
    } else {
        throw status;
    }
}
