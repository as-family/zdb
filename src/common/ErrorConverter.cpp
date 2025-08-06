#include "ErrorConverter.hpp"
#include "proto/error.pb.h"
#include <spdlog/spdlog.h>
#include <google/protobuf/any.pb.h>
#include <stdexcept>
#include <grpcpp/support/status.h>
#include "common/Error.hpp"

namespace zdb {

grpc::StatusCode toGrpcStatusCode(const ErrorCode& code) {
    switch (code) {
        case ErrorCode::NotFound:
            return grpc::StatusCode::NOT_FOUND;
        case ErrorCode::InvalidArg:
            return grpc::StatusCode::INVALID_ARGUMENT;
        default:
            return grpc::StatusCode::UNKNOWN;
    }
}

grpc::Status toGrpcStatus(const Error& error) {
    protoError::ErrorDetails details;
    details.set_code(static_cast<protoError::ErrorCode>(error.code));
    details.set_what(error.what);
    google::protobuf::Any anyDetail;
    anyDetail.PackFrom(details);
    return grpc::Status(toGrpcStatusCode(error.code), toString(error.code), anyDetail.SerializeAsString());
}

Error toError(const grpc::Status& status) {
    ErrorCode code;
    switch (status.error_code()) {
        case grpc::StatusCode::OK:
            spdlog::error("Attempted to convert OK gRPC status to Error. Throwing logic_error.");
            throw std::logic_error("Cannot convert OK status to Error");
        case grpc::StatusCode::NOT_FOUND:
            code = ErrorCode::NotFound;
            break;
        case grpc::StatusCode::INVALID_ARGUMENT:
            code = ErrorCode::InvalidArg;
            break;
        case grpc::StatusCode::UNAVAILABLE:
            code = ErrorCode::ServiceTemporarilyUnavailable;
            break;
        default:
            code = ErrorCode::Unknown;
    }
    spdlog::warn("gRPC call failed with status: {} - {}", static_cast<int>(status.error_code()), status.error_message());
    return Error(code, status.error_message());
}

} // namespace zdb
