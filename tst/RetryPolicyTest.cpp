#include <gtest/gtest.h>
#include <chrono>
#include <stdexcept>
#include "common/RetryPolicy.hpp"

using zdb::RetryPolicy;

TEST(RetryPolicyTest, ValidConstruction) {
    const RetryPolicy policy(
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(5000),
        3,
        2
    );
    EXPECT_EQ(policy.baseDelay, std::chrono::microseconds(100));
    EXPECT_EQ(policy.maxDelay, std::chrono::microseconds(1000));
    EXPECT_EQ(policy.resetTimeout, std::chrono::microseconds(5000));
    EXPECT_EQ(policy.failureThreshold, 3);
    EXPECT_EQ(policy.servicesToTry, 2);
}

TEST(RetryPolicyTest, NegativeThresholdThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(100),
            std::chrono::microseconds(1000),
            std::chrono::microseconds(5000),
            -1,
            1
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, NegativeBaseDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(-100),
            std::chrono::microseconds(1000),
            std::chrono::microseconds(5000),
            3,
            1
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, NegativeMaxDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(100),
            std::chrono::microseconds(-1000),
            std::chrono::microseconds(5000),
            3,
            1
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, NegativeResetTimeoutThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(100),
            std::chrono::microseconds(1000),
            std::chrono::microseconds(-5000),
            3,
            1
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, MaxDelayLessThanBaseDelayThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(1000),
            std::chrono::microseconds(100),
            std::chrono::microseconds(5000),
            3,
            1
        ),
        std::invalid_argument
    );
}

TEST(RetryPolicyTest, ZeroValuesAreAccepted) {
    const RetryPolicy policy(
        std::chrono::microseconds(0),
        std::chrono::microseconds(0),
        std::chrono::microseconds(0),
        0,
        0
    );
    EXPECT_EQ(policy.baseDelay, std::chrono::microseconds(0));
    EXPECT_EQ(policy.maxDelay, std::chrono::microseconds(0));
    EXPECT_EQ(policy.resetTimeout, std::chrono::microseconds(0));
    EXPECT_EQ(policy.failureThreshold, 0);
    EXPECT_EQ(policy.servicesToTry, 0);
}

TEST(RetryPolicyTest, NegativeServicesToTryThrows) {
    EXPECT_THROW(
        RetryPolicy(
            std::chrono::microseconds(100),
            std::chrono::microseconds(1000),
            std::chrono::microseconds(5000),
            3,
            -1
        ),
        std::invalid_argument
    );
}
