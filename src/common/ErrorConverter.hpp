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
#ifndef ERROR_CONVERTER_H
#define ERROR_CONVERTER_H

#include "common/Error.hpp"
#include "common/Types.hpp"
#include <grpcpp/support/status.h>
#include <expected>
#include <optional>
#include <variant>

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

std::expected<std::monostate, Error> toExpected(const grpc::Status& status);

ErrorCode errorCode(const zdb::Error& err);
ErrorCode errorCode(const std::expected<zdb::Value, zdb::Error>& result);
ErrorCode errorCode(const std::expected<std::monostate, zdb::Error>& result);

} // namespace zdb

#endif // ERROR_CONVERTER_H
