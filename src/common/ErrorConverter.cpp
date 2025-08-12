#include "common/ErrorConverter.hpp"
#include "proto/error.pb.h"
#include <spdlog/spdlog.h>
#include <google/protobuf/any.pb.h>
#include <stdexcept>
#include <grpcpp/support/status.h>
#include "common/Error.hpp"

namespace zdb {

grpc::StatusCode toGrpcStatusCode(const ErrorCode& code) {
    switch (code) {
        case ErrorCode::KeyNotFound:
            return grpc::StatusCode::NOT_FOUND;
        case ErrorCode::InvalidArg:
            return grpc::StatusCode::INVALID_ARGUMENT;
        case ErrorCode::VersionMismatch:
            return grpc::StatusCode::ABORTED;
        case ErrorCode::ServiceTemporarilyUnavailable:
            return grpc::StatusCode::UNAVAILABLE;
        case ErrorCode::AllServicesUnavailable:
            return grpc::StatusCode::UNAVAILABLE;
        default:
            return grpc::StatusCode::UNKNOWN;
    }
}

grpc::Status toGrpcStatus(const Error& error) {
    proto::ErrorDetails details;
    details.set_code(static_cast<proto::ErrorCode>(error.code));
    details.set_what(error.what);
    google::protobuf::Any anyDetail;
    anyDetail.PackFrom(details);
    return grpc::Status(toGrpcStatusCode(error.code), toString(error.code), anyDetail.SerializeAsString());
}

Error toError(const grpc::Status& status) {
    ErrorCode code = ErrorCode::Unknown;
    switch (status.error_code()) {
        case grpc::StatusCode::OK:
            spdlog::error("Attempted to convert OK gRPC status to error. Throwing logic_error.");
            throw std::logic_error("Cannot convert OK status to error");
        case grpc::StatusCode::NOT_FOUND:
            code = ErrorCode::KeyNotFound;
            break;
        case grpc::StatusCode::INVALID_ARGUMENT:
            code = ErrorCode::InvalidArg;
            break;
        case grpc::StatusCode::ABORTED:
            code = ErrorCode::VersionMismatch;
            break;
        case grpc::StatusCode::UNAVAILABLE:
            code = ErrorCode::ServiceTemporarilyUnavailable;
            break;
        default:
            code = ErrorCode::Unknown;
    }
    return Error(code, status.error_message());
}

ErrorCode errorCode(const zdb::Error& err) {
    return err.code;
}
ErrorCode errorCode(const std::expected<zdb::Value, zdb::Error>& result) {
    if (result.has_value()) {
        return ErrorCode::OK;
        throw std::logic_error("Expected error but got value");
    }
    return errorCode(result.error());
}
ErrorCode errorCode(const std::expected<void, zdb::Error>& result) {
    if (result.has_value()) {
        return ErrorCode::OK;
        throw std::logic_error("Expected error but got value");
    }
    return errorCode(result.error());
}

} // namespace zdb
