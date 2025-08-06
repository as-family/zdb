#include <gtest/gtest.h>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "common/Repeater.hpp"
#include "common/RetryPolicy.hpp"

using namespace zdb;

static grpc::Status retriableError() {
    // Simulate a retriable error (e.g., UNAVAILABLE)
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Retriable");
}

static grpc::Status nonRetriableError() {
    // Simulate a non-retriable error (e.g., INVALID_ARGUMENT)
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Non-retriable");
}

static grpc::Status okStatus() {
    return grpc::Status::OK;
}

TEST(RepeaterTest, SuccessOnFirstAttempt) {
    const RetryPolicy Policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 3, 0};
    Repeater repeater(Policy);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return okStatus();
    };
    auto status = repeater.attempt(rpc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(callCount, 1);
}

TEST(RepeaterTest, PermanentFailureNonRetriable) {
    const RetryPolicy Policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 3, 0};
    Repeater repeater(Policy);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return nonRetriableError();
    };
    auto status = repeater.attempt(rpc);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST(RepeaterTest, RetriableFailureThenSuccess) {
    const RetryPolicy Policy{std::chrono::microseconds(10), std::chrono::microseconds(100), std::chrono::microseconds(1000), 5, 0};
    Repeater repeater(Policy);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        if (callCount < 3) {
            return retriableError();
        }
        return okStatus();
    };
    auto status = repeater.attempt(rpc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(callCount, 3);
}

TEST(RepeaterTest, RetriableFailureExceedsThreshold) {
    const RetryPolicy Policy{std::chrono::microseconds(10), std::chrono::microseconds(100), std::chrono::microseconds(1000), 2, 0};
    Repeater repeater(Policy);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto status = repeater.attempt(rpc);
    EXPECT_FALSE(status.ok());
    EXPECT_GE(callCount, 2); // Should not retry more than threshold
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
}

TEST(RepeaterTest, ZeroThresholdNoRetry) {
    const RetryPolicy Policy{std::chrono::microseconds(10), std::chrono::microseconds(100), std::chrono::microseconds(1000), 0, 0};
    Repeater repeater(Policy);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto status = repeater.attempt(rpc);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(callCount, 1);
}

TEST(RepeaterTest, ZeroDelayNoSleep) {
    const RetryPolicy Policy{std::chrono::microseconds(0), std::chrono::microseconds(0), std::chrono::microseconds(0), 2, 0};
    Repeater repeater(Policy);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        if (callCount < 2) {
            return retriableError();
        }
        return okStatus();
    };
    auto start = std::chrono::steady_clock::now();
    auto status = repeater.attempt(rpc);
    auto end = std::chrono::steady_clock::now();
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(callCount, 2);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 10);
}
