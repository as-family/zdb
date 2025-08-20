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
            return grpc::StatusCode::INVALID_ARGUMENT;
        case ErrorCode::ServiceTemporarilyUnavailable:
            return grpc::StatusCode::UNAVAILABLE;
        case ErrorCode::AllServicesUnavailable:
            return grpc::StatusCode::UNAVAILABLE;
        case ErrorCode::TimeOut:
            return grpc::StatusCode::DEADLINE_EXCEEDED;
        default:
            return grpc::StatusCode::UNKNOWN;
    }
}

grpc::Status toGrpcStatus(const Error& error) {
    proto::ErrorDetails details;
    details.set_code(static_cast<proto::ErrorCode>(error.code));
    details.set_what(error.what);
    details.set_key(error.key);
    details.set_value(error.value);
    details.set_version(error.version);
    google::protobuf::Any anyDetail;
    anyDetail.PackFrom(details);
    return grpc::Status(toGrpcStatusCode(error.code), toString(error.code), anyDetail.SerializeAsString());
}

Error toError(const grpc::Status& status) {
    if (status.error_code() == grpc::StatusCode::OK) {
        throw std::logic_error("Cannot convert OK status to error");
    }
    ErrorCode code = ErrorCode::Unknown;
    proto::ErrorDetails details;
    google::protobuf::Any any;
    if (any.ParseFromString(status.error_details())) {
        if (any.UnpackTo(&details)) {
            code = static_cast<ErrorCode>(details.code());
            return Error(code, details.what(), details.key(), details.value(), details.version());
        }
    }
    switch (status.error_code()) {
        case grpc::StatusCode::NOT_FOUND:
            code = ErrorCode::KeyNotFound;
            break;
        case grpc::StatusCode::INVALID_ARGUMENT:
            code = ErrorCode::InvalidArg;
            break;
        case grpc::StatusCode::UNAVAILABLE:
            code = ErrorCode::ServiceTemporarilyUnavailable;
            break;
        case grpc::StatusCode::DEADLINE_EXCEEDED:
            code = ErrorCode::TimeOut;
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
        throw std::logic_error("Expected error but got value");
    }
    return errorCode(result.error());
}
ErrorCode errorCode(const std::expected<void, zdb::Error>& result) {
    if (result.has_value()) {
        throw std::logic_error("Expected error but got value");
    }
    return errorCode(result.error());
}

} // namespace zdb
