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
#include "common/Types.hpp"
#include <vector>

namespace zdb {

class KVStoreClient {
public:
    explicit KVStoreClient(Config& c);
    KVStoreClient(const KVStoreClient&) = delete;
    KVStoreClient& operator=(const KVStoreClient&) = delete;
    [[nodiscard]] std::expected<Value, Error> get(const Key& key) const;
    std::expected<void, Error> set(const Key& key, const Value& value);
    [[nodiscard]] std::expected<Value, Error> erase(const Key& key);
    [[nodiscard]] std::expected<size_t, Error> size() const;
private:
    template<typename Req, typename Rep>
    std::expected<void, std::vector<Error>> call(
        grpc::Status (kvStore::KVStoreService::Stub::* f)(grpc::ClientContext*, const Req&, Rep*),
        const Req& request,
        Rep& reply) const {
        for (int i = 0; i < config.policy.servicesToTry; ++i) {
            auto serviceResult = config.nextService();
            if (serviceResult.has_value()) {
                auto callResult = serviceResult.value()->call(f, request, reply);
                if (callResult.has_value()) {
                    return {};
                } else {
                    if (!isRetriable(callResult.error().back().code)) {
                        return callResult;
                    }
                }
            }
        }
        return std::unexpected {std::vector {Error(ErrorCode::AllServicesUnavailable, "All services are unavailable")}};
    }
    Config& config;
};

} // namespace zdb

#endif // KV_STORE_CLIENT_H
