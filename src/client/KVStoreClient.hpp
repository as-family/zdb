#ifndef KV_STORE_CLIENT_H
#define KV_STORE_CLIENT_H

#include <string>
#include <memory>
#include <expected>
#include <optional>
#include <vector>
#include <unordered_map>
#include "common/Error.hpp"
#include "common/RetryPolicy.hpp"
#include "client/Config.hpp"
#include <spdlog/spdlog.h>

namespace zdb {

class KVStoreClient {
public:
    KVStoreClient(Config& c);
    KVStoreClient(const KVStoreClient&) = delete;
    KVStoreClient& operator=(const KVStoreClient&) = delete;
    std::expected<std::string, Error> get(const std::string key) const;
    std::expected<void, Error> set(const std::string key, const std::string value);
    std::expected<std::string, Error> erase(const std::string key);
    std::expected<size_t, Error> size() const;
private:
    template<typename Req, typename Rep>
    std::expected<void, Error> call(
        grpc::Status (kvStore::KVStoreService::Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req& request,
        Rep& reply) const {
        for (int i = 0; i < config.policy.servicesToTry; ++i) {
            auto serviceResult = config.currentService();
            if (serviceResult.has_value()) {
                auto callResult = serviceResult.value()->call(f, request, reply);
                if (callResult.has_value()) {
                    return {};
                } else {
                    if (isRetriable(callResult.error().code)) {
                        config.nextService();
                    } else {
                        return callResult;
                    }
                    
                }
            } else {
                return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "All services are unavailable")};
            }
        }
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "All services are unavailable")};
    }
    Config& config;
};

} // namespace zdb

#endif // KV_STORE_CLIENT_H
