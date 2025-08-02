#ifndef ERROR_CONVERTER_H
#define ERROR_CONVERTER_H

#include "Error.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/any.pb.h>
#include <expected>
#include <optional>

namespace zdb {

grpc::StatusCode toGrpcStatusCode(ErrorCode code);

grpc::Status toGrpcStatus(const Error& error);

template<typename T>
grpc::Status toGrpcStatus(std::expected<T, Error>&& v) {
    if (v.has_value()) {
        return grpc::Status::OK;
    }
    return toGrpcStatus(v.error());
}

Error toError(grpc::Status status);

template<typename T>
std::expected<T, Error> toExpected(const grpc::Status& status, T v) {
    if (status.ok()) {
        return v;
    }
    return std::unexpected {toError(status)};
}

template<typename T>
std::expected<void, Error> toExpected(const grpc::Status& status) {
    if (status.ok()) {
        return {};
    }
    return std::unexpected {toError(status)};
}

} // namespace zdb

#endif // ERROR_CONVERTER_H
