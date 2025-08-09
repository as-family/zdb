#include "client/KVStoreClient.hpp"
#include <spdlog/spdlog.h>
#include "client/Config.hpp"
#include <string>
#include "proto/kvStore.pb.h"
#include "proto/kvStore.grpc.pb.h"
#include "common/Error.hpp"
#include <cstddef>
#include <expected>

namespace zdb {

KVStoreClient::KVStoreClient(Config& c) : config(c) {}

std::expected<std::string, Error> KVStoreClient::get(const std::string& key) const {
    kvStore::GetRequest request;
    request.set_key(key);
    kvStore::GetReply reply;
    auto t = call(
        &kvStore::KVStoreService::Stub::get,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.value();
    } else {
        spdlog::error("Failed to get value for key '{}': {}", key, t.error().what);
        return std::unexpected {t.error()};
    }
}

std::expected<void, Error> KVStoreClient::set(const std::string& key, const std::string& value) {
    kvStore::SetRequest request;
    request.set_key(key);
    request.set_value(value);
    kvStore::SetReply reply;
    auto t = call(
        &kvStore::KVStoreService::Stub::set,
        request,
        reply
    );
    if (t.has_value()) {
        return {};
    } else {
        spdlog::error("Failed to set value for key '{}': {}", key, t.error().what);
        return std::unexpected {t.error()};
    }
}

std::expected<std::string, Error> KVStoreClient::erase(const std::string& key) {
    kvStore::EraseRequest request;
    request.set_key(key);
    kvStore::EraseReply reply;
    auto t = call(
        &kvStore::KVStoreService::Stub::erase,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.value();
    } else {
        spdlog::error("Failed to erase value for key '{}': {}", key, t.error().what);
        return std::unexpected {t.error()};
    }
}

std::expected<size_t, Error> KVStoreClient::size() const {
    const kvStore::SizeRequest request;
    kvStore::SizeReply reply;
    auto t = call(
        &kvStore::KVStoreService::Stub::size,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.size();
    } else {
        spdlog::error("Failed to get size: {}", t.error().what);
        return std::unexpected {t.error()};
    }
}

} // namespace zdb
