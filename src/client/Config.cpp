#include "client/Config.hpp"
#include <spdlog/spdlog.h>
#include <vector>
#include <string>
#include "common/RetryPolicy.hpp"
#include <utility>
#include <stdexcept>
#include "common/Error.hpp"
#include <tuple>
#include <expected>

namespace zdb {

Config::iterator Config::nextActiveServiceIterator() {
    // First check if current service has become available again (e.g., after circuit breaker reset)
    if (cService != services.end()) {
        if (!cService->second.connected()) {
            if (cService->second.connect().has_value() && cService->second.available()) {
                return cService;
            }
        } else if (cService->second.available()) {
            return cService;
        }
    }
    
    // Then check other services
    for (auto i = services.begin(); i != services.end(); ++i) {
        if (i == cService) {
            continue;
        }
        if (!i->second.connected()) {
            if (i->second.connect().has_value() && i->second.available()) {
                return i;
            }
        } else if (i->second.available()) {
            return i;
        }
    }
    return services.end();
}

Config::Config(const std::vector<std::string>& addresses, const RetryPolicy p) : policy{p} {
    for (auto address : addresses) {
        services.emplace(std::piecewise_construct, 
                        std::forward_as_tuple(address), 
                        std::forward_as_tuple(address, p));
    }
    cService = services.end();
    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        throw std::runtime_error("KVStoreClient: Could not connect to any server"); 
    }
}

std::expected<KVRPCService*, Error> Config::currentService() {
    if (cService == services.end()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No service available")};
    }
    
    // Check if current service is available (circuit breaker not open)
    if (!cService->second.available()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "Current service not available")};
    }
    
    return &(cService->second);
}

std::expected<KVRPCService*, Error> Config::nextService() {
    // Check if current service is both connected and available (circuit breaker not open)
    if (cService != services.end() && cService->second.connected() && cService->second.available()) {
        return &(cService->second);
    }
    
    // Try to find an active service (including potentially reconnecting to current service)
    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        spdlog::warn("KVStoreClient: No available services left. ");
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
    }
    return &(cService->second);
}

} // namespace zdb
