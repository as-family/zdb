#include "KVRPCService.hpp"

#include<chrono>
#include <spdlog/spdlog.h>
#include <string>
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include <grpcpp/grpcpp.h>
#include "proto/kvStore.grpc.pb.h"
#include <grpcpp/security/credentials.h>

namespace zdb {

KVRPCService::KVRPCService(const std::string& address, const RetryPolicy& p) 
    : addr {address},
    circuitBreaker {p} {}

std::expected<void, Error> KVRPCService::connect() {
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(1))) {
        spdlog::warn("Could not connect to service @ {}", addr);
        return std::unexpected {Error{ErrorCode::Unknown, "Could not connect to service @" + addr}};
    }
    stub = kvStore::KVStoreService::NewStub(channel);
    spdlog::info("Connected to service @ {}", addr);
    return {};
}

bool KVRPCService::available() const {
    return !circuitBreaker.open();
}

bool KVRPCService::connected() const {
    return channel != nullptr && stub != nullptr;
}

std::string KVRPCService::address() const {
    return addr;
}

} // namespace zdb
