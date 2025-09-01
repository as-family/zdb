#ifndef PROXYSERVICE_HPP
#define PROXYSERVICE_HPP

#include <memory>
#include <grpcpp/grpcpp.h>
#include "KVTestFramework/NetworkConfig.hpp"
#include <expected>
#include <vector>
#include "common/Error.hpp"
#include <thread>
#include <string>
#include "common/ErrorConverter.hpp"
#include <vector>
#include <string>
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <mutex>
#include <condition_variable>
#include "common/RetryPolicy.hpp"

template<typename Service>
class ProxyService {
    using Stub = typename Service::Stub;
public:
    ProxyService(const std::string& original, NetworkConfig& c, zdb::RetryPolicy p)
    : originalAddress {original},
      networkConfig {c},
      policy {p} {}

    template<typename Req, typename Rep>
    std::expected<std::monostate, std::vector<zdb::Error>> call(
        std::string op,
        grpc::Status (Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req& request,
        Rep& reply) {
        std::lock_guard l{m};
        std::ignore = op;
        if (!networkConfig.isConnected()) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Disconnected"))});
        }
        if (!stub || !channel) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not Connected"))});
        }
        grpc::ClientContext c;
        c.set_deadline(std::chrono::system_clock::now() + policy.rpcTimeout);
        if (networkConfig.isReliable()) {
            auto status = (stub.get()->*f)(&c, request, &reply);
            if (status.ok()) {
                return {};
            } else {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(status)});
            }
        } else {
            if (networkConfig.shouldDrop()) {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped"))});
            }
            auto status = (stub.get()->*f)(&c, request, &reply);
            if (networkConfig.shouldDrop()) {
               return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped"))});
            }
            if (networkConfig.shouldDelay()) {
                std::this_thread::sleep_for(networkConfig.delayTime());
            }
            if (status.ok()) {
                return {};
            } else {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(status)});
            }
        }
    }
    NetworkConfig& getNetworkConfig() {
        return networkConfig;
    }
    void connectTarget() {
        std::lock_guard l{m};
        channel = grpc::CreateChannel(originalAddress, grpc::InsecureChannelCredentials());
        if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds(500))) {
            throw std::runtime_error("Failed to connect to channel");
        }
        stub = Service::NewStub(channel);
        if(!channel || !stub || channel->GetState(false) != grpc_connectivity_state::GRPC_CHANNEL_READY) {
            throw std::runtime_error("Failed to create channel or stub");
        }
    }
    std::string address() const {
        return originalAddress;
    }
private:
    std::string originalAddress;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<Stub> stub;
    NetworkConfig& networkConfig;
    zdb::RetryPolicy policy;
    std::mutex m{};
};

#endif // PROXYSERVICE_HPP
