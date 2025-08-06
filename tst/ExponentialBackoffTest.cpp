#include <gtest/gtest.h>
#include <chrono>
#include <stdexcept>
#include <vector>
#include <cstddef>  // for size_t
#include "common/ExponentialBackoff.hpp"
#include "common/RetryPolicy.hpp"

using namespace zdb;

class ExponentialBackoffTest : public ::testing::Test {
protected:
    RetryPolicy defaultPolicy{
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(0),
        5,
        0
    };
};

TEST_F(ExponentialBackoffTest, InitialDelayIsBaseDelay) {
    ExponentialBackoff backoff(defaultPolicy);
    auto delay = backoff.nextDelay();
    ASSERT_TRUE(delay.has_value());
    if (delay.has_value()) {
        EXPECT_EQ(delay.value(), std::chrono::microseconds(100));
    }
}

TEST_F(ExponentialBackoffTest, DelayDoublesEachAttempt) {
    ExponentialBackoff backoff(defaultPolicy);
    std::vector<unsigned int> expected = {100, 200, 400, 800, 1000}; // capped at maxDelay
    for (int i = 0; i < defaultPolicy.failureThreshold; ++i) {
        auto delay = backoff.nextDelay();
        ASSERT_TRUE(delay.has_value());
        if (delay.has_value()) {
            EXPECT_EQ(delay.value(), std::chrono::microseconds(expected[static_cast<size_t>(i)]));
        }
    }
}

TEST_F(ExponentialBackoffTest, DelayIsCappedAtMaxDelay) {
    const RetryPolicy Policy{
        std::chrono::microseconds(300),
        std::chrono::microseconds(500),
        std::chrono::microseconds(0),
        4,
        0
    };
    ExponentialBackoff backoff(Policy);
    std::vector<unsigned int> expected = {300, 500, 500, 500};
    for (int i = 0; i < Policy.failureThreshold; ++i) {
        auto delay = backoff.nextDelay();
        ASSERT_TRUE(delay.has_value());
        if (delay.has_value()) {
            EXPECT_EQ(delay.value(), std::chrono::microseconds(expected[static_cast<size_t>(i)]));
        }
    }
}

TEST_F(ExponentialBackoffTest, ReturnsNulloptAfterThreshold) {
    ExponentialBackoff backoff(defaultPolicy);
    for (int i = 0; i < defaultPolicy.failureThreshold; ++i) {
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
        EXPECT_EQ(delay.value(), std::chrono::microseconds(100));
    }
}

TEST_F(ExponentialBackoffTest, ZeroThresholdReturnsNulloptImmediately) {
    const RetryPolicy Policy{
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(0),
        0,
        0
    };
    ExponentialBackoff backoff(Policy);
    auto delay = backoff.nextDelay();
    EXPECT_FALSE(delay.has_value());
}


TEST_F(ExponentialBackoffTest, MaxDelayLessThanBaseDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(1000),
            std::chrono::microseconds(100),
            std::chrono::microseconds(0),
            2,
            0
        ),
        std::invalid_argument
    );
}

TEST_F(ExponentialBackoffTest, LargeAttemptDoesNotOverflow) {
    const RetryPolicy Policy{
        std::chrono::microseconds(1),
        std::chrono::microseconds(1000000),
        std::chrono::microseconds(0),
        30, // 1 << 30 is large
        0
    };
    ExponentialBackoff backoff(Policy);
    for (int i = 0; i < Policy.failureThreshold; ++i) {
        auto delay = backoff.nextDelay();
        ASSERT_TRUE(delay.has_value());
        if (delay.has_value()) {
            EXPECT_LE(delay.value(), std::chrono::microseconds(1000000));
        }
    }
}
