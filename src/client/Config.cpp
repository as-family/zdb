#include "client/Config.hpp"
#include <spdlog/spdlog.h>
#include <vector>
#include <string>
#include "common/RetryPolicy.hpp"
#include <utility>
#include <stdexcept>
#include "common/Error.hpp"
#include <tuple>

namespace zdb {

Config::iterator Config::nextActiveServiceIterator() {
    for (auto i = services.begin(); i != services.end(); ++i) {
        if (i == cService) {
            continue;
        }
        if (!i->second.connected()) {
            if (i->second.connect().has_value()) {
                return i;
            }
        } else if (i->second.available()) {
            return i;
        }
    }
    return services.end();
}

Config::Config(const std::vector<std::string>& addresses, const RetryPolicy& p) : policy{p} {
    for (auto address : addresses) {
        services.emplace(std::piecewise_construct, 
                        std::forward_as_tuple(address), 
                        std::forward_as_tuple(address, p));
    }
    cService = services.end();
    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        spdlog::error("KVStoreClient: Could not connect to any server. Throwing runtime_error.");
        throw std::runtime_error("KVStoreClient: Could not connect to any server"); 
    }
}

std::expected<KVRPCService*, Error> Config::currentService() const {
    if (cService == services.end()) {
        spdlog::error("No current service available. Throwing runtime_error.");
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No service available")};
    }
    return &(cService->second);
}

std::expected<KVRPCService*, Error> Config::nextService() {
    if (cService->second.available()) {
        return &(cService->second);
    }
    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        spdlog::error("KVStoreClient: No available services left. Throwing runtime_error.");
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
    }
    return &(cService->second);
}

} // namespace zdb
