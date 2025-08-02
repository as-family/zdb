#include "ErrorConverter.hpp"
#include "proto/error.pb.h"

namespace zdb {

grpc::StatusCode toGrpcStatusCode(ErrorCode code) {
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
    google::protobuf::Any any_detail;
    any_detail.PackFrom(details);
    return grpc::Status(toGrpcStatusCode(error.code), toString(error.code), any_detail.SerializeAsString());
}

Error toError(grpc::Status status) {
    ErrorCode code;
    switch (status.error_code()) {
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
