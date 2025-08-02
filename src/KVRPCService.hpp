#ifndef KVRPC_SERVICE_H
#define KVRPC_SERVICE_H

#include "CircuitBreaker.hpp"
#include "Repeater.hpp"
#include "Error.hpp"
#include "ErrorConverter.hpp"
#include <grpcpp/grpcpp.h>
#include "src/proto/kvStore.grpc.pb.h"
#include <expected>
#include <memory>
#include <functional>


namespace zdb {

class KVRPCService {
public:
    KVRPCService(const std::string s_address, const RetryPolicy& p);
    std::expected<void, Error> connect();
    template<typename Req, typename Rep>
    std::expected<void, Error> call(
        grpc::Status (kvStore::KVStoreService::Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req& request,
        Rep& reply) {
        std::function<grpc::Status()> bound = [this, f, request, &reply] {
            auto c = grpc::ClientContext();
            return (stub.get()->*f)(&c, request, &reply);
        };
        auto status = circuitBreaker.call(bound);
        if (status.ok()) {
            return {};
        } else {
            return std::unexpected {toError(status)};
        }
    }
    bool isAvailable();
private:
    std::string address;
    CircuitBreaker circuitBreaker;
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<kvStore::KVStoreService::Stub> stub;
};

} // namespace zdb

#endif // KVRPC_SERVICE_H
