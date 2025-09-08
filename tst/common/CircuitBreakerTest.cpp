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
#include <gtest/gtest.h>
#include <chrono>
#include <functional>
#include <thread>
#include <grpcpp/support/status.h>
#include "common/CircuitBreaker.hpp"
#include "common/RetryPolicy.hpp"
#include <vector>

using zdb::CircuitBreaker;
using zdb::RetryPolicy;

class CircuitBreakerTest : public ::testing::Test {
protected:
    RetryPolicy policy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 2, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    CircuitBreaker breaker{policy};
};

TEST_F(CircuitBreakerTest, InitialStateClosed) {
    EXPECT_FALSE(breaker.open());
}

TEST_F(CircuitBreakerTest, SuccessfulCallKeepsClosed) {
    const std::function<grpc::Status()> rpc = [] { return grpc::Status::OK; };
    auto status = breaker.call("get", rpc);
    EXPECT_TRUE(status.back().ok());
    EXPECT_FALSE(breaker.open());
}

TEST_F(CircuitBreakerTest, RetriableFailureOpensBreaker) {
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    auto status = breaker.call("get", rpc);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_TRUE(breaker.open());
}

TEST_F(CircuitBreakerTest, NonRetriableFailureKeepsClosed) {
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "fail");
    };
    auto status = breaker.call("get", rpc);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_FALSE(breaker.open());
}

TEST_F(CircuitBreakerTest, OpenBreakerBlocksCalls) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    // Should block
    auto status = breaker.call("get", rpc);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_EQ(status.back().error_message(), "Circuit breaker is open");
}

TEST_F(CircuitBreakerTest, HalfOpenAllowsTestCall) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    // Wait for resetTimeout
    std::this_thread::sleep_for(policy.resetTimeout);
    // Should transition to HalfOpen and call rpc
    bool called = false;
    const std::function<grpc::Status()> testRpc = [&called] {
        called = true;
        return grpc::Status::OK;
    };
    auto status = breaker.call("get", testRpc);
    EXPECT_TRUE(called);
    EXPECT_TRUE(status.back().ok());
    EXPECT_FALSE(breaker.open());
}

TEST_F(CircuitBreakerTest, HalfOpenFailureReopensBreaker) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    // Wait for resetTimeout
    std::this_thread::sleep_for(policy.resetTimeout);
    // Should transition to HalfOpen and call rpc
    const std::function<grpc::Status()> failRpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail again");
    };
    auto status = breaker.call("get", failRpc);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_TRUE(breaker.open());
}

TEST_F(CircuitBreakerTest, HalfOpenNonRetriableFailureClosesBreaker) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    // Wait for resetTimeout
    std::this_thread::sleep_for(policy.resetTimeout);
    // Should transition to HalfOpen and call rpc
    const std::function<grpc::Status()> nonRetriableRpc = [] {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "fail");
    };
    auto status = breaker.call("get", nonRetriableRpc);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_FALSE(breaker.open());
}

TEST_F(CircuitBreakerTest, MultipleFailuresTriggerOpen) {
    int failCount = 0;
    const std::function<grpc::Status()> rpc = [&failCount] {
        ++failCount;
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    for (int i = 0; i < policy.failureThreshold; ++i) {
        breaker.call("get", rpc);
    }
    EXPECT_TRUE(breaker.open());
}

TEST_F(CircuitBreakerTest, RapidCallsRespectTimeout) {
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    // Rapid call should still be blocked
    auto status = breaker.call("get", rpc);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_EQ(status.back().error_message(), "Circuit breaker is open");
}

// Edge case: call with nullptr
TEST_F(CircuitBreakerTest, NullptrCallThrows) {
    const std::function<grpc::Status()> rpc = nullptr;
    EXPECT_THROW(breaker.call("get", rpc), std::bad_function_call);
}

// Test that open() can transition state from Open to HalfOpen
TEST_F(CircuitBreakerTest, OpenTransitionsToHalfOpenAfterTimeout) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    
    // Wait for reset timeout
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    
    // Calling open() should transition to HalfOpen and return false
    EXPECT_FALSE(breaker.open());
    
    // Subsequent call to open() should still return false (still in HalfOpen)
    EXPECT_FALSE(breaker.open());
}

// Test open() side effects: multiple calls after timeout
TEST_F(CircuitBreakerTest, OpenSideEffectsMultipleCalls) {
    // Open the breaker
    const std::function<grpc::Status()> failRpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", failRpc);
    EXPECT_TRUE(breaker.open());
    
    // Wait for reset timeout
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    
    // Multiple calls to open() should consistently return false after transition
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(breaker.open());
    }
}

// Test that open() doesn't transition before timeout
TEST_F(CircuitBreakerTest, OpenDoesNotTransitionBeforeTimeout) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    
    // Don't wait for timeout - should still be open
    EXPECT_TRUE(breaker.open());
    
    // Wait a short time but less than reset timeout
    std::this_thread::sleep_for(policy.resetTimeout / 2);
    EXPECT_TRUE(breaker.open());
}

// Test circuit breaker state transitions with open() calls
TEST_F(CircuitBreakerTest, StateTransitionsWithOpenCalls) {
    // Initial state: Closed
    EXPECT_FALSE(breaker.open());
    
    // Trigger failure to open
    const std::function<grpc::Status()> failRpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", failRpc);
    EXPECT_TRUE(breaker.open()); // Now Open
    
    // Wait for reset timeout
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    
    // open() should transition to HalfOpen
    EXPECT_FALSE(breaker.open()); // Now HalfOpen
    
    // Make a successful call to close the breaker
    const std::function<grpc::Status()> successRpc = [] {
        return grpc::Status::OK;
    };
    auto status = breaker.call("get", successRpc);
    EXPECT_TRUE(status.back().ok());
    EXPECT_FALSE(breaker.open()); // Now Closed
}

// Test open() behavior with very short reset timeout
TEST_F(CircuitBreakerTest, OpenWithShortResetTimeout) {
    const RetryPolicy shortTimeoutPolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{1L}, 2, 2, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}}; // 1 microsecond reset timeout
    CircuitBreaker shortTimeoutBreaker{shortTimeoutPolicy};
    
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    shortTimeoutBreaker.call("get", rpc);
    EXPECT_TRUE(shortTimeoutBreaker.open());
    
    // Even a tiny sleep should trigger transition
    std::this_thread::sleep_for(std::chrono::microseconds{10L});
    EXPECT_FALSE(shortTimeoutBreaker.open());
}

// Test that open() state transition affects subsequent call() behavior
TEST_F(CircuitBreakerTest, OpenStateTransitionAffectsCallBehavior) {
    // Open the breaker
    const std::function<grpc::Status()> failRpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", failRpc);
    EXPECT_TRUE(breaker.open());
    
    // Immediate call should be blocked
    auto blockedStatus = breaker.call("get", failRpc);
    EXPECT_EQ(blockedStatus.back().error_code(), grpc::StatusCode::UNAVAILABLE);
    EXPECT_EQ(blockedStatus.back().error_message(), "Circuit breaker is open");
    
    // Wait for reset timeout
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    
    // Call open() to transition to HalfOpen
    EXPECT_FALSE(breaker.open());
    
    // Now calls should go through (not be blocked)
    bool called = false;
    const std::function<grpc::Status()> testRpc = [&called] {
        called = true;
        return grpc::Status::OK;
    };
    auto status = breaker.call("get", testRpc);
    EXPECT_TRUE(called);
    EXPECT_TRUE(status.back().ok());
}

// Test open() with concurrent access simulation
TEST_F(CircuitBreakerTest, OpenConcurrentAccessSimulation) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    
    // Wait for reset timeout
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    
    // Simulate multiple threads checking open() status
    std::vector<bool> results;
    for (int i = 0; i < 10; ++i) {
        results.push_back(breaker.open());
    }
    
    // All should return false (HalfOpen state)
    for (const bool result : results) {
        EXPECT_FALSE(result);
    }
}

// Test edge case: exactly at timeout boundary
TEST_F(CircuitBreakerTest, OpenAtTimeoutBoundary) {
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", rpc);
    EXPECT_TRUE(breaker.open());
    
    // Wait for exactly the reset timeout
    std::this_thread::sleep_for(policy.resetTimeout);
    
    // Should transition to HalfOpen
    EXPECT_FALSE(breaker.open());
}

// Test that open() doesn't affect Closed state
TEST_F(CircuitBreakerTest, OpenDoesNotAffectClosedState) {
    // Breaker starts in Closed state
    EXPECT_FALSE(breaker.open());
    
    // Multiple calls to open() when Closed should not change anything
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(breaker.open());
    }
    
    // Should still be able to make successful calls
    const std::function<grpc::Status()> successRpc = [] {
        return grpc::Status::OK;
    };
    auto status = breaker.call("get", successRpc);
    EXPECT_TRUE(status.back().ok());
    EXPECT_FALSE(breaker.open());
}

// Test open() behavior after HalfOpen failure
TEST_F(CircuitBreakerTest, OpenAfterHalfOpenFailure) {
    // Open the breaker
    const std::function<grpc::Status()> failRpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    breaker.call("get", failRpc);
    EXPECT_TRUE(breaker.open());
    
    // Wait for reset timeout and transition to HalfOpen
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    EXPECT_FALSE(breaker.open()); // Transitions to HalfOpen
    
    // Fail in HalfOpen state (should reopen)
    auto status = breaker.call("get", failRpc);
    EXPECT_FALSE(status.back().ok());
    EXPECT_TRUE(breaker.open()); // Should be Open again
}

// Test non-const nature of open() method
TEST_F(CircuitBreakerTest, OpenIsNonConst) {
    // This test verifies that open() can modify state
    CircuitBreaker& nonConstBreaker = breaker;
    
    // Open the breaker
    const std::function<grpc::Status()> rpc = [] {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "fail");
    };
    nonConstBreaker.call("get", rpc);
    EXPECT_TRUE(nonConstBreaker.open());
    
    // Wait and call open() - should modify internal state
    std::this_thread::sleep_for(policy.resetTimeout + std::chrono::milliseconds{10L});
    EXPECT_FALSE(nonConstBreaker.open());
    
    // Verify the state was actually modified by the open() call
    // (if it were const, this transition wouldn't happen)
    EXPECT_FALSE(nonConstBreaker.open());
}
