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
