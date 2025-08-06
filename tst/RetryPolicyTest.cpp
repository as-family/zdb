#include <gtest/gtest.h>
#include <chrono>
#include <stdexcept>
#include "common/RetryPolicy.hpp"

using namespace zdb;

TEST(RetryPolicyTest, ValidConstruction) {
    const RetryPolicy POLICY(
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(5000),
        3,
        2
    );
    EXPECT_EQ(POLICY.baseDelay, std::chrono::microseconds(100));
    EXPECT_EQ(POLICY.maxDelay, std::chrono::microseconds(1000));
    EXPECT_EQ(POLICY.resetTimeout, std::chrono::microseconds(5000));
    EXPECT_EQ(POLICY.failureThreshold, 3);
    EXPECT_EQ(POLICY.servicesToTry, 2);
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
    const RetryPolicy POLICY(
        std::chrono::microseconds(0),
        std::chrono::microseconds(0),
        std::chrono::microseconds(0),
        0,
        0
    );
    EXPECT_EQ(POLICY.baseDelay, std::chrono::microseconds(0));
    EXPECT_EQ(POLICY.maxDelay, std::chrono::microseconds(0));
    EXPECT_EQ(POLICY.resetTimeout, std::chrono::microseconds(0));
    EXPECT_EQ(POLICY.failureThreshold, 0);
    EXPECT_EQ(POLICY.servicesToTry, 0);
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
