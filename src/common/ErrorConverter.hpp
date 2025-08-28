#ifndef ERROR_CONVERTER_H
#define ERROR_CONVERTER_H

#include "common/Error.hpp"
#include "common/Types.hpp"
#include <grpcpp/support/status.h>
#include <expected>
#include <optional>

namespace zdb {

grpc::StatusCode toGrpcStatusCode(const ErrorCode& code);

grpc::Status toGrpcStatus(const Error& error);

template<typename T>
grpc::Status toGrpcStatus(const std::expected<T, Error>& v) {
    if (v.has_value()) {
        return grpc::Status::OK;
    }
    return toGrpcStatus(v.error());
}

Error toError(const grpc::Status& status);

template<typename T>
std::expected<T, Error> toExpected(const grpc::Status& status, T v) {
    if (status.ok()) {
        return v;
    }
    return std::unexpected {toError(status)};
}

template<typename T>
std::expected<std::monostate, Error> toExpected(const grpc::Status& status) {
    if (status.ok()) {
        return {};
    }
    return std::unexpected {toError(status)};
}

ErrorCode errorCode(const zdb::Error& err);
ErrorCode errorCode(const std::expected<zdb::Value, zdb::Error>& result);
ErrorCode errorCode(const std::expected<std::monostate, zdb::Error>& result);

} // namespace zdb

#endif // ERROR_CONVERTER_H
