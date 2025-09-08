// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "common/ErrorConverter.hpp"
#include "proto/error.pb.h"
#include <spdlog/spdlog.h>
#include <google/protobuf/any.pb.h>
#include <stdexcept>
#include <grpcpp/support/status.h>
#include "common/Error.hpp"
#include "common/Types.hpp"

namespace zdb {

grpc::StatusCode toGrpcStatusCode(const ErrorCode& code) {
    switch (code) {
        case ErrorCode::KeyNotFound:
            return grpc::StatusCode::NOT_FOUND;

        case ErrorCode::InvalidArg:
        case ErrorCode::VersionMismatch:
            return grpc::StatusCode::INVALID_ARGUMENT;

        case ErrorCode::ServiceTemporarilyUnavailable:
        case ErrorCode::AllServicesUnavailable:
            return grpc::StatusCode::UNAVAILABLE;

        case ErrorCode::Timeout:
            return grpc::StatusCode::DEADLINE_EXCEEDED;

        case ErrorCode::NotLeader:
            return grpc::StatusCode::FAILED_PRECONDITION;

        case ErrorCode::Cancelled:
            return grpc::StatusCode::CANCELLED;

        default:
            return grpc::StatusCode::UNKNOWN;
    }
}

std::expected<std::monostate, Error> toExpected(const grpc::Status& status) {
    if (status.ok()) {
        return {};
    }
    return std::unexpected {toError(status)};
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
            code = ErrorCode::Timeout;
            break;
        case grpc::StatusCode::FAILED_PRECONDITION:
            code = ErrorCode::NotLeader;
            break;
        case grpc::StatusCode::CANCELLED:
            code = ErrorCode::Cancelled;
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
ErrorCode errorCode(const std::expected<std::monostate, zdb::Error>& result) {
    if (result.has_value()) {
        throw std::logic_error("Expected error but got value");
    }
    return errorCode(result.error());
}

} // namespace zdb
