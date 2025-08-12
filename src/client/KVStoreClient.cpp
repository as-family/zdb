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
    
    bool is_retry = false;
    const int max_retries = 10;
    Error last_error{ErrorCode::Unknown, "No attempts made"};
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        auto t = call(
            &kvStore::KVStoreService::Stub::set,
            request,
            reply
        );
        
        if (t.has_value()) {
            // Success
            return {};
        } else {
            const auto& error = t.error();
            last_error = error;
            
            // According to lab 2: if this is a retry and we get ErrVersion,
            // we can't tell if the original request succeeded, so return ErrMaybe
            if (is_retry && error.code == ErrorCode::VersionMismatch) {
                spdlog::error("Failed to set value for key '{}' on retry: Version Mismatch (converting to Maybe)", key.data);
                return std::unexpected{Error{ErrorCode::Maybe, "Put may have succeeded on previous attempt"}};
            }
            
            // If this is the initial attempt and we get ErrVersion, return it as-is
            if (!is_retry && error.code == ErrorCode::VersionMismatch) {
                spdlog::error("Failed to set value for key '{}': {}", key.data, error.what);
                return std::unexpected{error};
            }
            
            // For other non-retriable errors, return immediately
            if (!isRetriable(error.code)) {
                spdlog::error("Failed to set value for key '{}': {}", key.data, error.what);
                return std::unexpected{error};
            }
            
            // For retriable errors, mark as retry and continue
            is_retry = true;
            spdlog::warn("Retrying set for key '{}' due to retriable error: {}", key.data, error.what);
            
            // Sleep a bit before retrying (as suggested in lab)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // All retries exhausted - return Maybe only if we had at least one successful
    // network communication that might have succeeded
    if (is_retry && (last_error.code == ErrorCode::ServiceTemporarilyUnavailable)) {
        spdlog::error("Failed to set value for key '{}' after {} retries - returning Maybe", key.data, max_retries);
        return std::unexpected{Error{ErrorCode::Maybe, "Request may have succeeded but no confirmation received"}};
    } else {
        spdlog::error("Failed to set value for key '{}' after {} retries", key.data, max_retries);
        return std::unexpected{last_error};
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
