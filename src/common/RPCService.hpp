#ifndef RPC_SERVICE_H
#define RPC_SERVICE_H

#include "common/CircuitBreaker.hpp"
#include "common/Repeater.hpp"
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <grpcpp/grpcpp.h>
#include <expected>
#include <memory>
#include <functional>
#include <chrono>
#include <algorithm>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include "proto/kvStore.pb.h"

namespace zdb {

template<typename Service>
class RPCService {
public:
    using Stub = typename Service::Stub;
    using function_t = std::function<grpc::Status(Stub*, grpc::ClientContext*, const google::protobuf::Message&, google::protobuf::Message*)>;
    RPCService(const std::string& address, const RetryPolicy p, std::unordered_map<std::string, function_t> f);
    RPCService(const RPCService&) = delete;
    RPCService& operator=(const RPCService&) = delete;
    std::expected<std::monostate, Error> connect();
    template<typename Req, typename Rep>
    std::expected<std::monostate, std::vector<Error>> call(
        const std::string& op,
        const Req& request,
        Rep& reply) {
        std::lock_guard<std::mutex> lock {m};
        if (!connected()) {
            return std::unexpected {std::vector<Error>{Error{ErrorCode::ServiceTemporarilyUnavailable, "Not connected"}}};
        }
        auto bound = [this, f = functions[op], &request, &reply] {
            grpc::ClientContext c {};
            c.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(policy.rpcTimeout));
            return f(stub.get(), &c, static_cast<const google::protobuf::Message&>(request), static_cast<google::protobuf::Message*>(&reply));
        };
        auto statuses = circuitBreaker.call(op, bound);
        if (statuses.back().ok()) {
            return {};
        } else {
            std::vector<Error> errors(statuses.size(), Error(ErrorCode::Unknown, "Unknown error"));
            std::transform(statuses.begin(), statuses.end(), errors.begin(), [](const grpc::Status& s) {
                return toError(s);
            });
            return std::unexpected {errors};
        }
    }
    [[nodiscard]] bool available();
    [[nodiscard]] bool connected() const;
    [[nodiscard]] std::string address() const;
private:
    std::mutex m;
    std::string addr;
    RetryPolicy policy;
    CircuitBreaker circuitBreaker;
    std::unordered_map<std::string, function_t> functions;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<Stub> stub;
};

template<typename Service>
RPCService<Service>::RPCService(const std::string& address, const RetryPolicy p, std::unordered_map<std::string, function_t> f) 
    : addr {address},
      policy {p},
      circuitBreaker {p},
      functions {std::move(f)} {}

template<typename Service>
std::expected<std::monostate, Error> RPCService<Service>::connect() {
    std::lock_guard<std::mutex> lock {m};
    if (channel) {
        auto state = channel->GetState(false);
        if (state == grpc_connectivity_state::GRPC_CHANNEL_READY || state == grpc_connectivity_state::GRPC_CHANNEL_IDLE || state == grpc_connectivity_state::GRPC_CHANNEL_CONNECTING) {
            if (state == GRPC_CHANNEL_READY) {
                if (!stub) {
                    stub = Service::NewStub(channel);
                }
                return {};
            }
            if (channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds(policy.channelTimeout))) {
                if (!stub) {
                    stub = Service::NewStub(channel);
                }
                return {};
            }
        }
    }
    
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds(policy.channelTimeout))) {
        return std::unexpected {Error{ErrorCode::Unknown, "Could not connect to service @" + addr}};
    }
    stub = Service::NewStub(channel);
    return {};
}

template<typename Service>
bool RPCService<Service>::available() {
    if (circuitBreaker.open()) {
        return false;
    }
    if (!connected()) {
        auto result = connect();
        if (!result.has_value()) {
            return false;
        }
    }
    return true;
}

template<typename Service>
bool RPCService<Service>::connected() const {
    if (!channel || !stub) {
        return false;
    }
    auto s = channel->GetState(false);
    return  s == grpc_connectivity_state::GRPC_CHANNEL_READY;
}

template<typename Service>
std::string RPCService<Service>::address() const {
    return addr;
}

} // namespace zdb

#endif // RPC_SERVICE_H
