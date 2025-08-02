#include "common/CircuitBreaker.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "common/ErrorConverter.hpp"
#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include <chrono>
#include <thread>

using namespace zdb;

class CircuitBreakerTest : public ::testing::Test {
protected:
    RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2};
    CircuitBreaker breaker{policy};
};

TEST_F(CircuitBreakerTest, InitialStateClosed) {
    EXPECT_FALSE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, SuccessfulCallKeepsClosed) {
    std::function<grpc::Status()> rpc = [] { return grpc::Status::OK; };
    auto status = breaker.call(rpc);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, RetriableFailureOpensBreaker) {
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    auto status = breaker.call(rpc);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_TRUE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, NonRetriableFailureKeepsClosed) {
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "fail");
    };
    auto status = breaker.call(rpc);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_FALSE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, OpenBreakerBlocksCalls) {
    // Open the breaker
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call(rpc);
    EXPECT_TRUE(breaker.isOpen());
    // Should block
    auto status = breaker.call(rpc);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_EQ(status.error_message(), "Circuit breaker is open");
}

TEST_F(CircuitBreakerTest, HalfOpenAllowsTestCall) {
    // Open the breaker
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call(rpc);
    EXPECT_TRUE(breaker.isOpen());
    // Wait for resetTimeout
    std::this_thread::sleep_for(policy.resetTimeout);
    // Should transition to HalfOpen and call rpc
    bool called = false;
    std::function<grpc::Status()> testRpc = [&called] {
        called = true;
        return grpc::Status::OK;
    };
    auto status = breaker.call(testRpc);
    EXPECT_TRUE(called);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, HalfOpenFailureReopensBreaker) {
    // Open the breaker
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call(rpc);
    EXPECT_TRUE(breaker.isOpen());
    // Wait for resetTimeout
    std::this_thread::sleep_for(policy.resetTimeout);
    // Should transition to HalfOpen and call rpc
    std::function<grpc::Status()> failRpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail again");
    };
    auto status = breaker.call(failRpc);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_TRUE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, HalfOpenNonRetriableFailureClosesBreaker) {
    // Open the breaker
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call(rpc);
    EXPECT_TRUE(breaker.isOpen());
    // Wait for resetTimeout
    std::this_thread::sleep_for(policy.resetTimeout);
    // Should transition to HalfOpen and call rpc
    std::function<grpc::Status()> nonRetriableRpc = [] {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "fail");
    };
    auto status = breaker.call(nonRetriableRpc);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_FALSE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, MultipleFailuresTriggerOpen) {
    int failCount = 0;
    std::function<grpc::Status()> rpc = [&failCount] {
        ++failCount;
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    for (int i = 0; i < policy.failureThreshold; ++i) {
        breaker.call(rpc);
    }
    EXPECT_TRUE(breaker.isOpen());
}

TEST_F(CircuitBreakerTest, RapidCallsRespectTimeout) {
    std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call(rpc);
    EXPECT_TRUE(breaker.isOpen());
    // Rapid call should still be blocked
    auto status = breaker.call(rpc);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_EQ(status.error_message(), "Circuit breaker is open");
}

// Edge case: call with nullptr
TEST_F(CircuitBreakerTest, NullptrCallThrows) {
    std::function<grpc::Status()> rpc = nullptr;
    EXPECT_THROW(breaker.call(rpc), std::bad_function_call);
}
