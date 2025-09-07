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
#include <mutex>
#include <unordered_map>

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

Config::Config(const std::vector<std::string>& addresses, const RetryPolicy p, std::unordered_map<std::string, KVRPCService::function_t> f)
    : policy{p},
      rng{std::random_device{}()} {
    if (addresses.empty()) {
        throw std::invalid_argument("Config: No addresses provided");
    }
    dist = std::uniform_int_distribution<std::size_t>(0, addresses.size() - 1);
    for (auto address : addresses) {
        services.emplace(std::piecewise_construct, 
                        std::forward_as_tuple(address), 
                        std::forward_as_tuple(address, p, f));
    }
    cService = services.end();
}

std::expected<KVRPCServicePtr, Error> Config::nextService() {
    std::lock_guard lock{m};
    if (cService != services.end() && cService->second.available()) {
        return &(cService->second);
    }

    cService = nextActiveServiceIterator();
    if (cService == services.end()) {
        return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
    }
    return &(cService->second);
}


std::expected<KVRPCServicePtr, Error> Config::randomService() {
    std::lock_guard lock{m};
    for (size_t j = 0; j < 10 * services.size(); ++j) {
        auto i = std::next(services.begin(), static_cast<std::ptrdiff_t>(dist(rng)));
        if (i == cService) {
            continue;
        }
        if (i->second.available()) {
            cService = i;
            return &i->second;
        }
    }
    return std::unexpected {Error(ErrorCode::AllServicesUnavailable, "No available services left")};
}

} // namespace zdb
