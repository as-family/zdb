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
    // If we already have a channel, check if it's usable
    if (channel) {
        auto state = channel->GetState(false); // Don't try to connect yet
        if (state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE || state == GRPC_CHANNEL_CONNECTING) {
            // Channel is usable or might become usable, don't recreate
            if (state == GRPC_CHANNEL_READY) {
                // Ensure stub is created if it doesn't exist
                if (!stub) {
                    stub = kvStore::KVStoreService::NewStub(channel);
                }
                spdlog::debug("Service @ {} already connected", addr);
                return {};
            }
            // For IDLE or CONNECTING, trigger connection attempt
            if (channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(1))) {
                // Ensure stub is created if it doesn't exist
                if (!stub) {
                    stub = kvStore::KVStoreService::NewStub(channel);
                }
                spdlog::info("Reconnected to service @ {}", addr);
                return {};
            }
        }
        // If we reach here, the channel is in TRANSIENT_FAILURE or SHUTDOWN, so recreate
    }
    
    // Create new channel only if needed
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(1))) {
        spdlog::warn("Could not connect to service @ {}", addr);
        return std::unexpected {Error{ErrorCode::Unknown, "Could not connect to service @" + addr}};
    }
    stub = kvStore::KVStoreService::NewStub(channel);
    spdlog::info("Connected to service @ {}", addr);
    return {};
}

bool KVRPCService::available() {
    // Check circuit breaker state (this may transition from Open to HalfOpen if timeout elapsed)
    if (circuitBreaker.open()) {
        return false;
    }
    
    // If circuit breaker is not open but we're not connected, try to reconnect
    if (!connected()) {
        auto result = connect();
        if (!result.has_value()) {
            return false; // Connection failed, service is not available
        }
    }
    
    return true;
}

bool KVRPCService::connected() const {
    return channel && stub && channel->GetState(true) == grpc_connectivity_state::GRPC_CHANNEL_READY;
}

std::string KVRPCService::address() const {
    return addr;
}

} // namespace zdb
