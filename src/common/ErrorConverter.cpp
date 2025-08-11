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
        case ErrorCode::NotFound:
            return grpc::StatusCode::NOT_FOUND;
        case ErrorCode::InvalidArg:
            return grpc::StatusCode::INVALID_ARGUMENT;
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
    return Error(code, status.error_message());
}

} // namespace zdb
