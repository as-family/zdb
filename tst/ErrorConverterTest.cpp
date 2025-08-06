#include <gtest/gtest.h>
#include <expected>
#include <utility>
#include <grpcpp/grpcpp.h>
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"

using namespace zdb;

TEST(ErrorConverterTest, ToGrpcStatusCode_AllCodes) {
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::NotFound), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::InvalidArg), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::Unknown), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::ServiceTemporarilyUnavailable), grpc::StatusCode::UNKNOWN); // fallback
}

TEST(ErrorConverterTest, ToGrpcStatus_ValidError) {
    const Error Err(ErrorCode::NotFound, "not found");
    const grpc::Status status = toGrpcStatus(Err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::NotFound));
    EXPECT_FALSE(status.ok());
}

TEST(ErrorConverterTest, ToGrpcStatus_UnknownError) {
    const Error Err(ErrorCode::Unknown, "unknown error");
    const grpc::Status status = toGrpcStatus(Err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::Unknown));
}

TEST(ErrorConverterTest, ToGrpcStatus_FromExpected) {
    std::expected<int, Error> ok = 42;
    std::expected<int, Error> err = std::unexpected(Error(ErrorCode::InvalidArg, "bad arg"));
    EXPECT_EQ(toGrpcStatus(ok).error_code(), grpc::StatusCode::OK);
    const grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::InvalidArg));
}

TEST(ErrorConverterTest, ToError_AllGrpcCodes) {
    const grpc::Status NotFound(grpc::StatusCode::NOT_FOUND, "nf");
    const grpc::Status Invalid(grpc::StatusCode::INVALID_ARGUMENT, "inv");
    const grpc::Status Unavailable(grpc::StatusCode::UNAVAILABLE, "unavail");
    const grpc::Status Unknown(grpc::StatusCode::UNKNOWN, "unk");
    const Error e1 = toError(NotFound);
    const Error e2 = toError(Invalid);
    const Error e3 = toError(Unavailable);
    const Error e4 = toError(Unknown);
    EXPECT_EQ(e1.code, ErrorCode::NotFound);
    EXPECT_EQ(e2.code, ErrorCode::InvalidArg);
    EXPECT_EQ(e3.code, ErrorCode::ServiceTemporarilyUnavailable);
    EXPECT_EQ(e4.code, ErrorCode::Unknown);
    EXPECT_EQ(e1.what, "nf");
    EXPECT_EQ(e2.what, "inv");
    EXPECT_EQ(e3.what, "unavail");
    EXPECT_EQ(e4.what, "unk");
}

TEST(ErrorConverterTest, ToExpected_ValueAndError) {
    const grpc::Status Ok(grpc::StatusCode::OK, "");
    const grpc::Status Err(grpc::StatusCode::INVALID_ARGUMENT, "bad");
    auto v = toExpected(Ok, 123);
    EXPECT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), 123);
    auto e = toExpected(Err, 123);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::InvalidArg);
    EXPECT_EQ(e.error().what, "bad");
}

TEST(ErrorConverterTest, ToExpectedVoid) {
    const grpc::Status Ok(grpc::StatusCode::OK, "");
    const grpc::Status Err(grpc::StatusCode::NOT_FOUND, "nf");
    auto v = toExpected<void>(Ok);
    EXPECT_TRUE(v.has_value());
    auto e = toExpected<void>(Err);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::NotFound);
    EXPECT_EQ(e.error().what, "nf");
}
