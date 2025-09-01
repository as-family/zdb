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

namespace zdb {

KVStoreClient::KVStoreClient(Config& c) : config(c) {}

std::expected<Value, Error> KVStoreClient::get(const Key& key) const {
    kvStore::GetRequest request;
    request.mutable_key()->set_data(key.data);
    kvStore::GetReply reply;
    auto t = call(
        "get",
        &kvStore::KVStoreService::Stub::get,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.value();
    } else {
        return std::unexpected {t.error().back()};
    }
}

std::expected<std::monostate, Error> KVStoreClient::set(const Key& key, const Value& value) {
    kvStore::SetRequest request;
    request.mutable_key()->set_data(key.data);
    request.mutable_value()->set_data(value.data);
    request.mutable_value()->set_version(value.version);
    kvStore::SetReply reply;

    auto t = call(
        "set",
        &kvStore::KVStoreService::Stub::set,
        request,
        reply
    );

    if (t.has_value()) {
        return {};
    } else {
        if (t.error().size() > 1 && isRetriable("set", t.error().front().code) && t.error().back().code == ErrorCode::VersionMismatch) {
            return std::unexpected {Error(ErrorCode::Maybe)};
        } else {
            return std::unexpected {t.error().back()};
        }
    }
}

std::expected<Value, Error> KVStoreClient::erase(const Key& key) {
    kvStore::EraseRequest request;
    request.mutable_key()->set_data(key.data);
    kvStore::EraseReply reply;
    auto t = call(
        "erase",
        &kvStore::KVStoreService::Stub::erase,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.value();
    } else {
        return std::unexpected {t.error().back()};
    }
}

std::expected<size_t, Error> KVStoreClient::size() const {
    kvStore::SizeRequest request;
    kvStore::SizeReply reply;
    auto t = call(
        "size",
        &kvStore::KVStoreService::Stub::size,
        request,
        reply
    );
    if (t.has_value()) {
        return reply.size();
    } else {
        return std::unexpected {t.error().back()};
    }
}


void KVStoreClient::waitSet(Key key, Value value) {
    while(true) {
        auto v = set(key, value);
        if (v.has_value()) {
            return;
        } else {
            if (waitGet(key, Value{value.data, value.version + 1})) {
                return;
            }
        }
    }
    std::unreachable();
}

bool KVStoreClient::waitGet(Key key, Value value) {
    while (true) {
        auto t = get(key);
        if(t.has_value()) {
            if(t.value() == value) {
                return true;
            } else {
                return false;
            }
        } else {
            if (t.error().code == ErrorCode::KeyNotFound) {
                return false;
            }
        }
    }
    std::unreachable();
}

bool KVStoreClient::waitNotFound(Key key) {
    while (true) {
        auto t = get(key);
        if (t.has_value()) {
            return false;
        } else {
            if (t.error().code == ErrorCode::KeyNotFound) {
                return true;
            }
        }
    }
    std::unreachable();
}

Value KVStoreClient::waitGet(Key key, uint64_t version) {
    while (true) {
        auto t = get(key);
        if (t.has_value()) {
            if (t.value().version == version) {
                return t.value();
            }
        }
    }
    std::unreachable();
}

} // namespace zdb
