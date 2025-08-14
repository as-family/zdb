#ifndef KVRPC_SERVICE_H
#define KVRPC_SERVICE_H

#include "common/CircuitBreaker.hpp"
#include "common/Repeater.hpp"
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <grpcpp/grpcpp.h>
#include "proto/kvStore.grpc.pb.h"
#include <expected>
#include <memory>
#include <functional>
#include <chrono>

namespace zdb {

class KVRPCService {
public:
    KVRPCService(const std::string& address, const RetryPolicy& p);
    KVRPCService(const KVRPCService&) = delete;
    KVRPCService& operator=(const KVRPCService&) = delete;
    std::expected<void, Error> connect();
    template<typename Req, typename Rep>
    std::expected<void, Error> call(
        grpc::Status (kvStore::KVStoreService::Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req& request,
        Rep& reply) {
        auto bound = [this, f, &request, &reply] {
            auto c = grpc::ClientContext();
            c.set_deadline(std::chrono::system_clock::now() + std::chrono::microseconds(200));
            return (stub.get()->*f)(&c, request, &reply);
        };
        auto status = circuitBreaker.call(bound);
        if (status.ok()) {
            return {};
        } else {
            return std::unexpected {toError(status)};
        }
    }
    [[nodiscard]] bool available();
    [[nodiscard]] bool connected() const;
    [[nodiscard]] std::string address() const;
private:
    const std::string addr;
    CircuitBreaker circuitBreaker;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<kvStore::KVStoreService::Stub> stub;
};

} // namespace zdb

#endif // KVRPC_SERVICE_H
