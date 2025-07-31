#include "KVRPCService.hpp"

#include<chrono>

namespace zdb {

KVRPCService::KVRPCService(const std::string s_address, const RetryPolicy& p) 
    : address {s_address},
    circuitBreaker {p} {}

std::expected<void, Error> KVRPCService::connect() {
    channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(1))) {
        return std::unexpected {Error{ErrorCode::Unknown, "Could not connect to service @" + address}};
    }
    stub = kvStore::KVStoreService::NewStub(channel);
    return {};
}

bool KVRPCService::isAvailable() {
    return !circuitBreaker.isOpen();
}

} // namespace zdb
