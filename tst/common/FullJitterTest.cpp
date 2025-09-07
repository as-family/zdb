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
#include "common/FullJitter.hpp"
#include <chrono>
#include <set>
#include <cstdint>

using zdb::FullJitter;

TEST(FullJitterTest, ReturnsValueWithinRange) {
    FullJitter jitter;
    auto input = std::chrono::microseconds{1000L};
    for (int i = 0; i < 100; ++i) {
        auto result = jitter.jitter(input);
        EXPECT_GE(result.count(), 0);
        EXPECT_LE(result.count(), 1000);
    }
}

TEST(FullJitterTest, ZeroInputAlwaysReturnsZero) {
    FullJitter jitter;
    auto input = std::chrono::microseconds{0L};
    for (int i = 0; i < 10; ++i) {
        auto result = jitter.jitter(input);
        EXPECT_EQ(result.count(), 0);
    }
}

TEST(FullJitterTest, DistributionIsUniformEnough) {
    FullJitter jitter;
    auto input = std::chrono::microseconds{10L};
    std::set<int64_t> seen;
    for (int i = 0; i < 1000; ++i) {
        auto result = jitter.jitter(input);
        seen.insert(result.count());
    }
    // Should see most values between 0 and 10
    EXPECT_GE(seen.size(), 8);
}

TEST(FullJitterTest, LargeInputDoesNotOverflow) {
    FullJitter jitter;
    auto input = std::chrono::microseconds{1000000L};
    for (int i = 0; i < 10; ++i) {
        auto result = jitter.jitter(input);
        EXPECT_GE(result.count(), 0);
        EXPECT_LE(result.count(), 1000000);
    }
}

// Negative input is not supported by FullJitter, but test for robustness
TEST(FullJitterTest, NegativeInputThrowsOrReturnsZero) {
    FullJitter jitter;
    auto input = std::chrono::microseconds{-100};
    // Uniform distribution with negative max is undefined, so expect exception or zero
    try {
        auto result = jitter.jitter(input);
        // If no exception, expect zero
        EXPECT_EQ(result.count(), 0);
    } catch (...) {
        SUCCEED();
    }
}
