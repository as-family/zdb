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
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::NotFound), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::InvalidArg), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::Unknown), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::ServiceTemporarilyUnavailable), grpc::StatusCode::UNKNOWN); // fallback
}

TEST(ErrorConverterTest, ToGrpcStatusValidError) {
    const Error err(ErrorCode::NotFound, "not found");
    const grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::NotFound));
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
    EXPECT_EQ(e1.code, ErrorCode::NotFound);
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
    auto v = toExpected<void>(ok);
    EXPECT_TRUE(v.has_value());
    auto e = toExpected<void>(err);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::NotFound);
    EXPECT_EQ(e.error().what, "nf");
}
