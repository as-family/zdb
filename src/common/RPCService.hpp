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

namespace zdb {

template<typename Service>
class RPCService {
public:
    using Stub = typename Service::Stub;
    RPCService(const std::string& address, const RetryPolicy p);
    RPCService(const RPCService&) = delete;
    RPCService& operator=(const RPCService&) = delete;
    std::expected<void, Error> connect();
    template<typename Req, typename Rep>
    std::expected<void, std::vector<Error>> call(
        const std::string& op,
        grpc::Status (Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req& request,
        Rep& reply) {
        auto bound = [this, f, &request, &reply] {
            auto c = grpc::ClientContext();
            c.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(25));
            return (stub.get()->*f)(&c, request, &reply);
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
    const std::string addr;
    CircuitBreaker circuitBreaker;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<Stub> stub;
};

template<typename Service>
RPCService<Service>::RPCService(const std::string& address, const RetryPolicy p) 
    : addr {address},
    circuitBreaker {p} {}

template<typename Service>
std::expected<void, Error> RPCService<Service>::connect() {
    if (channel) {
        auto state = channel->GetState(false);
        if (state == grpc_connectivity_state::GRPC_CHANNEL_READY || state == grpc_connectivity_state::GRPC_CHANNEL_IDLE || state == grpc_connectivity_state::GRPC_CHANNEL_CONNECTING) {
            if (state == GRPC_CHANNEL_READY) {
                if (!stub) {
                    stub = Service::NewStub(channel);
                }
                return {};
            }
            if (channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds(1000))) {
                if (!stub) {
                    stub = Service::NewStub(channel);
                }
                return {};
            }
        }
    }
    
    channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds(1000))) {
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
    return channel && stub && channel->GetState(false) == grpc_connectivity_state::GRPC_CHANNEL_READY;
}

template<typename Service>
std::string RPCService<Service>::address() const {
    return addr;
}

} // namespace zdb

#endif // RPC_SERVICE_H
