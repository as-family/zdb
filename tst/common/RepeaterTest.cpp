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
#include "common/Repeater.hpp"
#include "common/RetryPolicy.hpp"
#include <grpcpp/support/status.h>

using zdb::Repeater;
using zdb::RetryPolicy;

namespace {
    grpc::Status retriableError() {
        // Simulate a retriable error (e.g., UNAVAILABLE)
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Retriable");
    }

    grpc::Status nonRetriableError() {
        // Simulate a non-retriable error (e.g., INVALID_ARGUMENT)
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Non-retriable");
    }

    grpc::Status okStatus() {
        return grpc::Status::OK;
    }
} // namespace

TEST(RepeaterTest, SuccessOnFirstAttempt) {
    const RetryPolicy policy{
        std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 3, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return okStatus();
    };
    auto status = repeater.attempt("get", rpc);
    EXPECT_TRUE(status.back().ok());
    EXPECT_EQ(callCount, 1);
}

TEST(RepeaterTest, PermanentFailureNonRetriable) {
    const RetryPolicy policy{
        std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 3, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return nonRetriableError();
    };
    auto status = repeater.attempt("get", rpc);
    EXPECT_FALSE(status.back().ok());
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST(RepeaterTest, RetriableFailureThenSuccess) {
    const RetryPolicy policy{
        std::chrono::microseconds{10L}, std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, 5, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        if (callCount < 3) {
            return retriableError();
        }
        return okStatus();
    };
    auto status = repeater.attempt("get", rpc);
    EXPECT_TRUE(status.back().ok());
    EXPECT_EQ(callCount, 3);
}

TEST(RepeaterTest, RetriableFailureExceedsThreshold) {
    const RetryPolicy policy{
        std::chrono::microseconds{10L}, std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, 2, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto status = repeater.attempt("get", rpc);
    EXPECT_FALSE(status.back().ok());
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(status.back().error_code(), grpc::StatusCode::UNAVAILABLE);
}

TEST(RepeaterTest, ZeroThresholdNoRetry) {
    const RetryPolicy policy{
        std::chrono::microseconds{10L}, std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, 0, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto status = repeater.attempt("get", rpc);
    EXPECT_FALSE(status.back().ok());
    EXPECT_EQ(callCount, 1);
}

TEST(RepeaterTest, ZeroDelayNoSleep) {
    const RetryPolicy policy{
        std::chrono::microseconds{0L}, std::chrono::microseconds{0L}, std::chrono::microseconds{0L}, 2, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        if (callCount < 2) {
            return retriableError();
        }
        return okStatus();
    };
    auto start = std::chrono::steady_clock::now();
    auto status = repeater.attempt("get", rpc);
    auto end = std::chrono::steady_clock::now();
    EXPECT_TRUE(status.back().ok());
    EXPECT_EQ(callCount, 2);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), 10);
}

// Measures the time spent in the repeater and compares to expected policy delay
TEST(RepeaterTest, TimeSpentMatchesPolicyDelay) {
    const int retries = 3;
    const std::chrono::microseconds baseDelay(50000); // 50ms
    const std::chrono::microseconds maxDelay(200000); // 200ms
    const RetryPolicy policy{
        baseDelay, maxDelay, std::chrono::microseconds{0L}, retries, 0, std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto start = std::chrono::steady_clock::now();
    auto status = repeater.attempt("get", rpc);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // Expected delay: sum of backoff delays (with jitter, so allow some tolerance)
    int64_t expectedMin = 0;
    int64_t expectedMax = 0;
    for (int i = 0; i < retries; ++i) {
        int64_t delay = std::min<int64_t>(baseDelay.count() * (1LL << i), static_cast<int64_t>(maxDelay.count()));
        expectedMin += 0; // FullJitter can be 0
        expectedMax += delay / 1000; // convert to ms
    }
    EXPECT_FALSE(status.back().ok());
    EXPECT_EQ(callCount, retries);
    EXPECT_GE(elapsed, 0);
    EXPECT_LE(elapsed, expectedMax + 100); // allow 100ms tolerance for thread scheduling
}

// Test negative delay handling in jitter (should throw)
TEST(RepeaterTest, NegativeDelayThrows) {
    zdb::FullJitter jitter;
    EXPECT_THROW(jitter.jitter(std::chrono::microseconds{-1}), std::invalid_argument);
}

// Test max threshold: should not exceed failureThreshold
TEST(RepeaterTest, MaxThresholdRespected) {
    const RetryPolicy policy{
        std::chrono::microseconds{10L}, std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, 1, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto status = repeater.attempt("get", rpc);
    EXPECT_FALSE(status.back().ok());
    EXPECT_EQ(callCount, 1);
}

// Test with large baseDelay and maxDelay
TEST(RepeaterTest, LargeDelays) {
    const RetryPolicy policy{
        std::chrono::microseconds{500000L}, std::chrono::microseconds{1000000L}, std::chrono::microseconds{0L}, 2, 0,
        std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}
    };
    std::atomic<bool> sc;
    Repeater repeater(policy, sc);
    int callCount = 0;
    auto rpc = [&]() {
        ++callCount;
        return retriableError();
    };
    auto start = std::chrono::steady_clock::now();
    auto status = repeater.attempt("get", rpc);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_FALSE(status.back().ok());
    EXPECT_EQ(callCount, 2);
    EXPECT_GE(elapsed, 0);
}
