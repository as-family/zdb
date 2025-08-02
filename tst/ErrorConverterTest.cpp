#include <gtest/gtest.h>
#include "common/ErrorConverter.hpp"
#include "common/Error.hpp"
#include <grpcpp/grpcpp.h>

using namespace zdb;

TEST(ErrorConverterTest, ToGrpcStatusCode_AllCodes) {
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::NotFound), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::InvalidArg), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::Unknown), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(toGrpcStatusCode(ErrorCode::ServiceTemporarilyUnavailable), grpc::StatusCode::UNKNOWN); // fallback
}

TEST(ErrorConverterTest, ToGrpcStatus_ValidError) {
    Error err(ErrorCode::NotFound, "not found");
    grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::NotFound));
    EXPECT_FALSE(status.ok());
}

TEST(ErrorConverterTest, ToGrpcStatus_UnknownError) {
    Error err(ErrorCode::Unknown, "unknown error");
    grpc::Status status = toGrpcStatus(err);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNKNOWN);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::Unknown));
}

TEST(ErrorConverterTest, ToGrpcStatus_FromExpected) {
    std::expected<int, Error> ok = 42;
    std::expected<int, Error> err = std::unexpected(Error(ErrorCode::InvalidArg, "bad arg"));
    EXPECT_EQ(toGrpcStatus(std::move(ok)).error_code(), grpc::StatusCode::OK);
    grpc::Status status = toGrpcStatus(std::move(err));
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_message(), toString(ErrorCode::InvalidArg));
}

TEST(ErrorConverterTest, ToError_AllGrpcCodes) {
    grpc::Status notFound(grpc::StatusCode::NOT_FOUND, "nf");
    grpc::Status invalid(grpc::StatusCode::INVALID_ARGUMENT, "inv");
    grpc::Status unavailable(grpc::StatusCode::UNAVAILABLE, "unavail");
    grpc::Status unknown(grpc::StatusCode::UNKNOWN, "unk");
    Error e1 = toError(notFound);
    Error e2 = toError(invalid);
    Error e3 = toError(unavailable);
    Error e4 = toError(unknown);
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
    grpc::Status ok(grpc::StatusCode::OK, "");
    grpc::Status err(grpc::StatusCode::INVALID_ARGUMENT, "bad");
    auto v = toExpected(ok, 123);
    EXPECT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), 123);
    auto e = toExpected(err, 123);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::InvalidArg);
    EXPECT_EQ(e.error().what, "bad");
}

TEST(ErrorConverterTest, ToExpectedVoid) {
    grpc::Status ok(grpc::StatusCode::OK, "");
    grpc::Status err(grpc::StatusCode::NOT_FOUND, "nf");
    auto v = toExpected<void>(ok);
    EXPECT_TRUE(v.has_value());
    auto e = toExpected<void>(err);
    EXPECT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code, ErrorCode::NotFound);
    EXPECT_EQ(e.error().what, "nf");
}
