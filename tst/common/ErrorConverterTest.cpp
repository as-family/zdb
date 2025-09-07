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
#include <gtest/gtest.h>
#include "common/Error.hpp"
#include <expected>
#include "common/ErrorConverter.hpp"
#include <grpcpp/support/status.h>

using zdb::ErrorCode;
using zdb::Error;
using zdb::toGrpcStatusCode;
using zdb::toGrpcStatus;
using zdb::toError;
using zdb::toExpected;

TEST(ErrorConverterTest, ToGrpcStatusCodeAllCodes) {
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::KeyNotFound), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::InvalidArg), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::Unknown), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::ServiceTemporarilyUnavailable), grpc::StatusCode::UNAVAILABLE);
}

TEST(ErrorConverterTest, ToGrpcStatusValidError) {
    const Error err(ErrorCode::KeyNotFound, "not found");
    const grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::KeyNotFound));
    EXPECT_FALSE(status.ok());
}

TEST(ErrorConverterTest, ToGrpcStatusUnknownError) {
    const Error err(ErrorCode::Unknown, "unknown error");
    const grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::Unknown));
}

TEST(ErrorConverterTest, ToGrpcStatusFromExpected) {
    const std::expected<int, Error> ok = 42;
    const std::expected<int, Error> err = std::unexpected(Error(ErrorCode::InvalidArg, "bad arg"));
    EXPECT_EQ(toGrpcStatus(ok).error_code(), grpc::StatusCode::OK);
    const grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::InvalidArg));
}

TEST(ErrorConverterTest, ToErrorAllGrpcCodes) {
    const grpc::Status notFound(grpc::StatusCode::NOT_FOUND, "nf");
    const grpc::Status invalid(grpc::StatusCode::INVALID_ARGUMENT, "inv");
    const grpc::Status unavailable(grpc::StatusCode::UNAVAILABLE, "unavail");
    const grpc::Status unknown(grpc::StatusCode::UNKNOWN, "unk");
    const Error e1 = toError(notFound);
    const Error e2 = toError(invalid);
    const Error e3 = toError(unavailable);
    const Error e4 = toError(unknown);
    EXPECT_EQ(e1.code, ErrorCode::KeyNotFound);
    EXPECT_EQ(e2.code, ErrorCode::InvalidArg);
    EXPECT_EQ(e3.code, ErrorCode::ServiceTemporarilyUnavailable);
    EXPECT_EQ(e4.code, ErrorCode::Unknown);
    EXPECT_EQ(e1.what, "nf");
    EXPECT_EQ(e2.what, "inv");
    EXPECT_EQ(e3.what, "unavail");
    EXPECT_EQ(e4.what, "unk");
}

TEST(ErrorConverterTest, ToExpectedValueAndError) {
    const grpc::Status ok(grpc::StatusCode::OK, "");
    const grpc::Status err(grpc::StatusCode::INVALID_ARGUMENT, "bad");
    auto v = toExpected(ok, 123);
    EXPECT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), 123);
    auto e = toExpected(err, 123);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::InvalidArg);
    EXPECT_EQ(e.error().what, "bad");
}

TEST(ErrorConverterTest, ToExpectedVoid) {
    const grpc::Status ok(grpc::StatusCode::OK, "");
    const grpc::Status err(grpc::StatusCode::NOT_FOUND, "nf");
    auto v = toExpected(ok);
    EXPECT_TRUE(v.has_value());
    auto e = toExpected(err);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::KeyNotFound);
    EXPECT_EQ(e.error().what, "nf");
}
