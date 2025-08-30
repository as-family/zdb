#include "client/Config.hpp"
#include <vector>
#include <string>
#include "common/RetryPolicy.hpp"
#include <utility>
#include <stdexcept>
#include "common/Error.hpp"
#include <tuple>
#include <expected>
#include "client/Config.hpp"

namespace zdb {

Config::iterator Config::nextActiveServiceIterator() {
    for (auto i = services.begin(); i != services.end(); ++i) {
        if (i == cService) {
            continue;
        }
        if (i->second.available()) {
            return i;
        }
    }
    return services.end();
}

Config::Config(const std::vector<std::string>& addresses, const RetryPolicy p) : policy{p}, used{} {
    for (auto address : addresses) {
        services.emplace(std::piecewise_construct, 
                        std::forward_as_tuple(address), 
                        std::forward_as_tuple(address, p));
        used[address] = false;
    }
    cService = services.end();
    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        throw std::runtime_error("Config: Could not connect to any server");
    }
}

std::expected<KVRPCServicePtr, Error> Config::currentService() {
    if (cService == services.end()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No service available")};
    }

    // Check if current service is available (circuit breaker not open)
    if (!cService->second.available()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "Current service not available")};
    }

    return &(cService->second);
}

std::expected<KVRPCServicePtr, Error> Config::nextService() {
    // Check if current service is both connected and available (circuit breaker not open)
    if (cService != services.end() && cService->second.available()) {
        return &(cService->second);
    }

    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
    }
    return &(cService->second);
}


std::expected<KVRPCServicePtr, Error> Config::forceNextService() {
    used[cService->first] = true;
    for (auto i = services.begin(); i != services.end(); ++i) {
        if (i == cService) {
            continue;
        }
        if (used[i->first]) {
            continue;
        }
        if (i->second.available()) {
            cService = i;
            return &i->second;
        }
    }
    return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
}

void Config::resetUsed() {
    for (auto& [key, _] : used) {
        used[key] = false;
    }
}

} // namespace zdb
