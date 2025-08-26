#ifndef PROXYSERVICE_HPP
#define PROXYSERVICE_HPP

#include <memory>
#include <grpcpp/grpcpp.h>
#include "KVTestFramework/NetworkConfig.hpp"
#include <string>
#include <thread>

template<typename Service>
class ProxyService {
    using Stub = typename Service::Stub;
public:
    ProxyService(const std::string original, NetworkConfig& c)
    : originalAddress{original},
      channel{grpc::CreateChannel(original, grpc::InsecureChannelCredentials())},
      networkConfig{c} {
        if (!channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::milliseconds(100))) {
            throw std::runtime_error("Failed to connect to channel");
        }
        stub = Service::NewStub(channel);
        if(!channel || !stub || channel->GetState(false) != grpc_connectivity_state::GRPC_CHANNEL_READY) {
            throw std::runtime_error("Failed to create channel or stub");
        }
    }

    template<typename Req, typename Rep>
    grpc::Status call(
        grpc::Status (Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req* request,
        Rep* reply) const {
        grpc::ClientContext c;
        if (networkConfig.reliable()) {
            auto status = (stub.get()->*f)(&c, *request, reply);
            return status;
        } else {
            if (networkConfig.drop()) {
                return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped");
            }
            auto status = (stub.get()->*f)(&c, *request, reply);
            if (networkConfig.drop()) {
               return grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped");
            }
            if (networkConfig.delay()) {
                std::this_thread::sleep_for(networkConfig.delayTime());
            }
            return status;
        }
    }

private:
    std::string originalAddress;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<Stub> stub;
    NetworkConfig& networkConfig;
};

#endif // PROXYSERVICE_HPP
