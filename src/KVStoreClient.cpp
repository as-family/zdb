#include "KVStoreClient.hpp"
#include "ErrorConverter.hpp"

namespace zdb {

KVStoreClient::KVStoreClient(const std::string s_address)
    : channel {grpc::CreateChannel(s_address, grpc::InsecureChannelCredentials())},
      stub {kvStore::KVStoreService::NewStub(channel)} {}
    
std::expected<std::optional<std::string>, Error> KVStoreClient::get(const std::string key) const {
    kvStore::GetRequest request;
    request.set_key(key);
    kvStore::GetReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->get(&context, request, &reply)};
    return toExpected<std::optional<std::string>>(status, reply.value());
}

std::expected<void, Error> KVStoreClient::set(const std::string key, const std::string value) {
    kvStore::SetRequest request;
    request.set_key(key);
    request.set_value(value);
    kvStore::SetReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->set(&context, request, &reply)};
    return toExpected<void>(status);
}

std::expected<std::optional<std::string>, Error> KVStoreClient::erase(const std::string key) {
    kvStore::EraseRequest request;
    request.set_key(key);
    kvStore::EraseReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->erase(&context, request, &reply)};
    return toExpected<std::optional<std::string>>(status, reply.value());
}

std::expected<size_t, Error> KVStoreClient::size() const {
    kvStore::SizeRequest request;
    kvStore::SizeReply reply;
    grpc::ClientContext context;
    grpc::Status status {stub->size(&context, request, &reply)};
    toExpected<size_t>(status, reply.size());
}

} // namespace zdb
