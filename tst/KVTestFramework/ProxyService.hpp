#ifndef PROXYSERVICE_HPP
#define PROXYSERVICE_HPP

#include <memory>
#include <grpcpp/grpcpp.h>
#include "KVTestFramework/NetworkConfig.hpp"
#include <expected>
#include <vector>
#include "common/Error.hpp"
#include <thread>
#include <vector>
#include <string>
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <mutex>
#include <condition_variable>
#include "common/RetryPolicy.hpp"
#include "common/RPCService.hpp"
#include <proto/kvStore.grpc.pb.h>
#include <proto/kvStore.pb.h>

inline std::unordered_map<std::string, typename zdb::RPCService<zdb::kvStore::KVStoreService>::function_t> getDefaultKVProxyFunctions() {
    return {
        { "get", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->get(ctx, static_cast<const zdb::kvStore::GetRequest&>(req), static_cast<zdb::kvStore::GetReply*>(resp));
        }},
        { "set", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->set(ctx, static_cast<const zdb::kvStore::SetRequest&>(req), static_cast<zdb::kvStore::SetReply*>(resp));
        }},
        { "erase", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->erase(ctx, static_cast<const zdb::kvStore::EraseRequest&>(req), static_cast<zdb::kvStore::EraseReply*>(resp));
        }},
        { "size", [](zdb::kvStore::KVStoreService::Stub* stub, grpc::ClientContext* ctx, const google::protobuf::Message& req, google::protobuf::Message* resp) -> grpc::Status {
            return stub->size(ctx, static_cast<const zdb::kvStore::SizeRequest&>(req), static_cast<zdb::kvStore::SizeReply*>(resp));
        }}
    };
}

template<typename Service>
class ProxyService {
    using Stub = typename Service::Stub;
public:
    ProxyService(const std::string& original, NetworkConfig& c, zdb::RetryPolicy p)
    : originalAddress {original},
      networkConfig {c},
      policy {p} {}

    ProxyService(const std::string& original, NetworkConfig& c, zdb::RetryPolicy p, std::unordered_map<std::string, typename zdb::RPCService<Service>::function_t> f)
    : originalAddress {original},
      functions {std::move(f)},
      networkConfig {c},
      policy {p} {}

    template<typename Req, typename Rep>
    std::expected<std::monostate, std::vector<zdb::Error>> call(
        std::string op,
        const Req& request,
        Rep& reply) {
        std::lock_guard l{m};
        if (!networkConfig.isConnected()) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Disconnected"))});
        }
        if (!stub || !channel) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNAVAILABLE, "Not Connected"))});
        }
        auto funcIt = functions.find(op);
        if (funcIt == functions.end()) {
            return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Unknown operation"))});
        }
        grpc::ClientContext c;
        c.set_deadline(std::chrono::system_clock::now() + policy.rpcTimeout);
        if (networkConfig.isReliable()) {
            auto status = funcIt->second(stub.get(), &c, static_cast<const google::protobuf::Message&>(request), static_cast<google::protobuf::Message*>(&reply));
            if (status.ok()) {
                return {};
            } else {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(status)});
            }
        } else {
            if (networkConfig.shouldDrop()) {
                return std::unexpected(std::vector<zdb::Error> {zdb::toError(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "Dropped"))});
            }
            auto status = funcIt->second(stub.get(), &c, static_cast<const google::protobuf::Message&>(request), static_cast<google::protobuf::Message*>(&reply));
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
    std::mutex m{};
    std::string originalAddress;
    std::unordered_map<std::string, typename zdb::RPCService<Service>::function_t> functions;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<Stub> stub;
    NetworkConfig& networkConfig;
    zdb::RetryPolicy policy;
};

#endif // PROXYSERVICE_HPP
