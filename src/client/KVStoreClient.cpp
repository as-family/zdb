#include "client/KVStoreClient.hpp"
#include <spdlog/spdlog.h>
#include "client/Config.hpp"
#include <string>
#include "proto/kvStore.pb.h"
#include "proto/kvStore.grpc.pb.h"
#include "common/Error.hpp"
#include "common/Types.hpp"
#include <cstddef>
#include <expected>
#include <thread>
#include <chrono>

namespace zdb {

KVStoreClient::KVStoreClient(Config& c) : config(c) {}

std::expected<Value, Error> KVStoreClient::get(const Key& key) const {
    kvStore::GetRequest request;
    request.mutable_key()->set_data(key.data);
    kvStore::GetReply reply;
    auto t = call(
        &kvStore::KVStoreService::Stub::get,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.value();
    } else {
        spdlog::error("Failed to get value for key '{}': {}", key.data, t.error().what);
        return std::unexpected {t.error()};
    }
}

std::expected<void, Error> KVStoreClient::set(const Key& key, const Value& value) {
    kvStore::SetRequest request;
    request.mutable_key()->set_data(key.data);
    request.mutable_value()->set_data(value.data);
    request.mutable_value()->set_version(value.version);
    kvStore::SetReply reply;

    auto t = call(
        &kvStore::KVStoreService::Stub::set,
        request,
        reply
    );

    if (t.has_value()) {
        return {};
    } else {
        spdlog::error("KVStoreClient::set: RPC call failed with error: {} (code: {})", 
                     t.error().what, static_cast<int>(t.error().code));
        if (t.error().code == ErrorCode::VersionMismatch) {
            return std::unexpected{Error(ErrorCode::Maybe, "Maybe there")};
        }
        return std::unexpected{t.error()};
    }
}

std::expected<Value, Error> KVStoreClient::erase(const Key& key) {
    kvStore::EraseRequest request;
    request.mutable_key()->set_data(key.data);
    kvStore::EraseReply reply;
    auto t = call(
        &kvStore::KVStoreService::Stub::erase,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.value();
    } else {
        spdlog::error("Failed to erase value for key '{}': {}", key.data, t.error().what);
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
