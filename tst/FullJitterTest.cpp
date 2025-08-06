#include <gtest/gtest.h>
#include "common/FullJitter.hpp"
#include <chrono>
#include <set>

using namespace zdb;

TEST(FullJitterTest, ReturnsValueWithinRange) {
    FullJitter jitter;
    auto input = std::chrono::microseconds(1000);
    for (int i = 0; i < 100; ++i) {
        auto result = jitter.jitter(input);
        EXPECT_GE(result.count(), 0);
        EXPECT_LE(result.count(), 1000);
    }
}

TEST(FullJitterTest, ZeroInputAlwaysReturnsZero) {
    FullJitter jitter;
    auto input = std::chrono::microseconds(0);
    for (int i = 0; i < 10; ++i) {
        auto result = jitter.jitter(input);
        EXPECT_EQ(result.count(), 0);
    }
}

TEST(FullJitterTest, DistributionIsUniformEnough) {
    FullJitter jitter;
    auto input = std::chrono::microseconds(10);
    std::set<long> seen;
    for (int i = 0; i < 1000; ++i) {
        auto result = jitter.jitter(input);
        seen.insert(result.count());
    }
    // Should see most values between 0 and 10
    EXPECT_GE(seen.size(), 8);
}

TEST(FullJitterTest, LargeInputDoesNotOverflow) {
    FullJitter jitter;
    auto input = std::chrono::microseconds(1000000);
    for (int i = 0; i < 10; ++i) {
        auto result = jitter.jitter(input);
        EXPECT_GE(result.count(), 0);
        EXPECT_LE(result.count(), 1000000);
    }
}

// Negative input is not supported by FullJitter, but test for robustness
TEST(FullJitterTest, NegativeInputThrowsOrReturnsZero) {
    FullJitter jitter;
    auto input = std::chrono::microseconds(-100);
    // Uniform distribution with negative max is undefined, so expect exception or zero
    try {
        auto result = jitter.jitter(input);
        // If no exception, expect zero
        EXPECT_EQ(result.count(), 0);
    } catch (...) {
        SUCCEED();
    }
}
