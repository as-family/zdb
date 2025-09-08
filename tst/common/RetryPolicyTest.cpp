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
#include <stdexcept>
#include "common/RetryPolicy.hpp"

using zdb::RetryPolicy;

TEST(RetryPolicyTest, ValidConstruction) {
    const RetryPolicy policy(
        std::chrono::microseconds{100L},
        std::chrono::microseconds{1000L},
        std::chrono::microseconds{5000L},
        3,
        2,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    );
    EXPECT_EQ(policy.baseDelay, std::chrono::microseconds{100L});
    EXPECT_EQ(policy.maxDelay, std::chrono::microseconds{1000L});
    EXPECT_EQ(policy.resetTimeout, std::chrono::microseconds{5000L});
    EXPECT_EQ(policy.failureThreshold, 3);
    EXPECT_EQ(policy.servicesToTry, 2);
}

TEST(RetryPolicyTest, NegativeThresholdThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{100L},
            std::chrono::microseconds{1000L},
            std::chrono::microseconds{5000L},
            -1,
            1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, NegativeBaseDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{-100},
            std::chrono::microseconds{1000L},
            std::chrono::microseconds{5000L},
            3,
            1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, NegativeMaxDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{100L},
            std::chrono::microseconds{-1000},
            std::chrono::microseconds{5000L},
            3,
            1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, NegativeResetTimeoutThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{100L},
            std::chrono::microseconds{1000L},
            std::chrono::microseconds{-5000},
            3,
            1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, MaxDelayLessThanBaseDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{1000L},
            std::chrono::microseconds{100L},
            std::chrono::microseconds{5000L},
            3,
            1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, ZeroValuesAreAccepted) {
    const RetryPolicy policy(
        std::chrono::microseconds{0L},
        std::chrono::microseconds{0L},
        std::chrono::microseconds{0L},
        0,
        0,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    );
    EXPECT_EQ(policy.baseDelay, std::chrono::microseconds{0L});
    EXPECT_EQ(policy.maxDelay, std::chrono::microseconds{0L});
    EXPECT_EQ(policy.resetTimeout, std::chrono::microseconds{0L});
    EXPECT_EQ(policy.failureThreshold, 0);
    EXPECT_EQ(policy.servicesToTry, 0);
}

TEST(RetryPolicyTest, NegativeServicesToTryThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds{100L},
            std::chrono::microseconds{1000L},
            std::chrono::microseconds{5000L},
            3,
            -1,
            std::chrono::milliseconds{1000L},
            std::chrono::milliseconds{200L}
        ),
        std::invalid_argument
    );
}
