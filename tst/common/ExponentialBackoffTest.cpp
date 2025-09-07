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
#include <stdexcept>
#include <vector>
#include <cstddef>
#include "common/ExponentialBackoff.hpp"
#include "common/RetryPolicy.hpp"

using zdb::ExponentialBackoff;
using zdb::RetryPolicy;

class ExponentialBackoffTest : public ::testing::Test {
protected:
    RetryPolicy defaultPolicy{
        std::chrono::microseconds{100L},
        std::chrono::microseconds{1000L},
        std::chrono::microseconds{0L},
        5,
        0,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
};

TEST_F(ExponentialBackoffTest, InitialDelayIsBaseDelay) {
    ExponentialBackoff backoff(defaultPolicy);
    auto delay = backoff.nextDelay();
    ASSERT_TRUE(delay.has_value());
    if (delay.has_value()) {
        EXPECT_EQ(delay.value(), std::chrono::microseconds{100L});
    }
}

TEST_F(ExponentialBackoffTest, DelayDoublesEachAttempt) {
    ExponentialBackoff backoff(defaultPolicy);
    std::vector<unsigned int> expected = {100, 200, 400, 800, 1000}; // capped at maxDelay
    for (int i = 0; i < defaultPolicy.failureThreshold - 1; ++i) {
        auto delay = backoff.nextDelay();
        ASSERT_TRUE(delay.has_value());
        if (delay.has_value()) {
            EXPECT_EQ(delay.value(), std::chrono::microseconds{expected[static_cast<size_t>(i)]});
        }
    }
}

TEST_F(ExponentialBackoffTest, DelayIsCappedAtMaxDelay) {
    const RetryPolicy policy{
        std::chrono::microseconds{300L},
        std::chrono::microseconds{500L},
        std::chrono::microseconds{0L},
        4,
        0,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    ExponentialBackoff backoff(policy);
    std::vector<unsigned int> expected = {300, 500, 500, 500};
    for (int i = 0; i < policy.failureThreshold - 1; ++i) {
        auto delay = backoff.nextDelay();
        ASSERT_TRUE(delay.has_value());
        if (delay.has_value()) {
            EXPECT_EQ(delay.value(), std::chrono::microseconds{expected[static_cast<size_t>(i)]});
        }
    }
}

TEST_F(ExponentialBackoffTest, ReturnsNulloptAfterThreshold) {
    ExponentialBackoff backoff(defaultPolicy);
    for (int i = 0; i < defaultPolicy.failureThreshold - 1; ++i) {
        ASSERT_TRUE(backoff.nextDelay().has_value());
    }
    auto delay = backoff.nextDelay();
    EXPECT_FALSE(delay.has_value());
}

TEST_F(ExponentialBackoffTest, ResetRestartsAttempts) {
    ExponentialBackoff backoff(defaultPolicy);
    for (int i = 0; i < defaultPolicy.failureThreshold; ++i) {
        backoff.nextDelay();
    }
    EXPECT_FALSE(backoff.nextDelay().has_value());
    backoff.reset();
    auto delay = backoff.nextDelay();
    EXPECT_TRUE(delay.has_value());
    if (delay.has_value()) {
        EXPECT_EQ(delay.value(), std::chrono::microseconds{100L});
    }
}

TEST_F(ExponentialBackoffTest, ZeroThresholdReturnsNulloptImmediately) {
    const RetryPolicy policy{
        std::chrono::microseconds{100L},
        std::chrono::microseconds{1000L},
        std::chrono::microseconds{0L},
        0,
        0,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    ExponentialBackoff backoff(policy);
    auto delay = backoff.nextDelay();
    EXPECT_FALSE(delay.has_value());
}


TEST_F(ExponentialBackoffTest, MaxDelayLessThanBaseDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{1000L},
            std::chrono::microseconds{100L},
            std::chrono::microseconds{0L},
            2,
            0,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}

TEST_F(ExponentialBackoffTest, LargeAttemptDoesNotOverflow) {
    const RetryPolicy policy{
        std::chrono::microseconds{1L},
        std::chrono::microseconds{1000000L},
        std::chrono::microseconds{0L},
        30, // 1 << 30 is large
        0,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    ExponentialBackoff backoff(policy);
    for (int i = 0; i < policy.failureThreshold - 1; ++i) {
        auto delay = backoff.nextDelay();
        ASSERT_TRUE(delay.has_value());
        if (delay.has_value()) {
            EXPECT_LE(delay.value(), std::chrono::microseconds{1000000L});
        }
    }
}
