#include "KVStoreClient.hpp"
#include "ErrorConverter.hpp"
#include <stdexcept>

namespace zdb {

KVStoreClient::KVStoreClient(const std::vector<std::string>& addresses, RetryPolicy& r)
    : services {}, currentService {services.end()} {
    for (auto a : addresses) {
        services.insert({a, KVRPCService {a, r}});
    }
    for (auto i = services.begin(); i != services.end(); ++i) {
        if (i->second.connect().has_value()) {
            currentService = i;
        }
    }
    if (currentService == services.end()) {
        throw std::runtime_error("KVStoreClient: Could not connect to any server"); 
    }
}

std::expected<std::optional<std::string>, Error> KVStoreClient::get(const std::string key) const {
    kvStore::GetRequest request;
    request.set_key(key);
    kvStore::GetReply reply;
    auto t = currentService->second.call(
        &kvStore::KVStoreService::Stub::get,
        request,
        &reply
    );
    if (t.has_value()) {
        return reply.value();
    }
    return std::unexpected {t.error()};
}

std::expected<void, Error> KVStoreClient::set(const std::string key, const std::string value) {
    kvStore::SetRequest request;
    request.set_key(key);
    request.set_value(value);
    kvStore::SetReply reply;
    auto t = currentService->second.call(
        &kvStore::KVStoreService::Stub::set,
        request,
        &reply
    );
    if (t.has_value()) {
        return {};
    }
    return std::unexpected {t.error()};
}

std::expected<std::optional<std::string>, Error> KVStoreClient::erase(const std::string key) {
    kvStore::EraseRequest request;
    request.set_key(key);
    kvStore::EraseReply reply;
    auto t = currentService->second.call(
        &kvStore::KVStoreService::Stub::erase,
        request,
        &reply
    );
    if (t.has_value()) {
        return reply.value();
    }
    return std::unexpected {t.error()};
}

std::expected<size_t, Error> KVStoreClient::size() const {
    kvStore::SizeRequest request;
    kvStore::SizeReply reply;
    auto t = currentService->second.call(
        &kvStore::KVStoreService::Stub::size,
        request,
        &reply
    );
    if (t.has_value()) {
        return reply.size();
    }
    return std::unexpected {t.error()};
}

} // namespace zdb
