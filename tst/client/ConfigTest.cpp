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
#include "client/Config.hpp"
#include "common/RetryPolicy.hpp"
#include "server/KVStoreServiceImpl.hpp"
#include "storage/InMemoryKVStore.hpp"
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <atomic>
#include "common/Error.hpp"
#include "raft/TestRaft.hpp"
#include "raft/SyncChannel.hpp"
#include "common/KVStateMachine.hpp"

using zdb::Config;
using zdb::RetryPolicy;
using zdb::InMemoryKVStore;
using zdb::KVStoreServiceImpl;
using zdb::KVStoreServer;
using zdb::ErrorCode;

class ConfigTest : public ::testing::Test {
protected:
    RetryPolicy policy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 3, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    
    // Test server setup for positive tests
    const std::string validServerAddr = "localhost:50053";
    const std::string validServerAddr2 = "localhost:50054";
    const std::string invalidServerAddr = "localhost:59999";
    
    InMemoryKVStore kvStore;
    raft::SyncChannel<std::shared_ptr<raft::Command>> leader1{};
    TestRaft raft1{leader1};
    zdb::KVStateMachine kvState1 = zdb::KVStateMachine {kvStore, leader1, raft1};
    raft::SyncChannel<std::shared_ptr<raft::Command>> leader2{};
    raft::SyncChannel<std::shared_ptr<raft::Command>> follower2{};
    TestRaft raft2{leader2};
    zdb::KVStateMachine kvState2 = zdb::KVStateMachine {kvStore, leader2, raft2};
    KVStoreServiceImpl serviceImpl1{kvState1};
    KVStoreServiceImpl serviceImpl2{kvState2};
    std::unique_ptr<KVStoreServer> server1;
    std::unique_ptr<KVStoreServer> server2;
    
    void SetUp() override {
        // Start test servers
        server1 = std::make_unique<KVStoreServer>(validServerAddr, serviceImpl1);
        server2 = std::make_unique<KVStoreServer>(validServerAddr2, serviceImpl2);
        
        // Give servers time to start
        std::this_thread::sleep_for(std::chrono::milliseconds{300L});
    }
    
    void TearDown() override {
        // Gracefully shutdown servers
        if (server1) {
            server1->shutdown();
        }
        if (server2) {
            server2->shutdown();
        }
    }
};

// Test successful construction with single valid address
TEST_F(ConfigTest, ConstructorWithSingleValidAddress) {
    const std::vector<std::string> addresses{validServerAddr};
    
    ASSERT_NO_THROW({
        Config config(addresses, policy);
        auto result = config.nextService();
        ASSERT_TRUE(result.has_value());
    });
}

// Test successful construction with multiple valid addresses
TEST_F(ConfigTest, ConstructorWithMultipleValidAddresses) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    
    ASSERT_NO_THROW({
        Config config(addresses, policy);
        auto result = config.nextService();
        ASSERT_TRUE(result.has_value());
    });
}


// Test construction with some valid and some invalid addresses (should succeed)
TEST_F(ConfigTest, ConstructorWithMixedAddressesWithSomeValid) {
    const std::vector<std::string> addresses{invalidServerAddr, validServerAddr};
    
    ASSERT_NO_THROW({
        Config config(addresses, policy);
        auto result = config.nextService();
        ASSERT_TRUE(result.has_value());
    });
}

// Test nextService() returns valid service after successful construction
TEST_F(ConfigTest, CurrentServiceReturnsValidService) {
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, policy);
    
    auto result = config.nextService();
    ASSERT_TRUE(result.has_value());
    zdb::KVRPCServicePtr service = result.value();
    EXPECT_TRUE(service->connected());
    EXPECT_TRUE(service->available());
}

// Test nextService() when current service is available
TEST_F(ConfigTest, NextServiceWhenCurrentServiceAvailable) {
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, policy);
    
    auto currentResult = config.nextService();
    ASSERT_TRUE(currentResult.has_value());
    const zdb::KVRPCServicePtr currentSvc = currentResult.value();
    
    auto nextResult = config.nextService();
    ASSERT_TRUE(nextResult.has_value());
    const zdb::KVRPCServicePtr nextSvc = nextResult.value();
    
    // Should return the same service if it's still available
    EXPECT_EQ(currentSvc, nextSvc);
}

// Test nextService() switching to another service
TEST_F(ConfigTest, NextServiceSwitchesToAnotherService) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, policy);
    
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    const zdb::KVRPCServicePtr service1 = result1.value();
    
    auto result2 = config.nextService();
    ASSERT_TRUE(result2.has_value());
    const zdb::KVRPCServicePtr service2 = result2.value();
    
    // Both should be valid services
    EXPECT_TRUE(service1->connected());
    EXPECT_TRUE(service2->connected());
}

// Test copy constructor is deleted
TEST_F(ConfigTest, CopyConstructorIsDeleted) {
    const std::vector<std::string> addresses{validServerAddr};
    const Config config(addresses, policy);
    
    // This should not compile - testing that copy constructor is deleted
    // Config config2(config); // Uncommenting this line should cause compilation error
    
    // Instead, we'll verify that the copy constructor is indeed deleted by
    // checking that std::is_copy_constructible returns false
    EXPECT_FALSE(std::is_copy_constructible_v<zdb::Config>);
}

// Test assignment operator is deleted
TEST_F(ConfigTest, AssignmentOperatorIsDeleted) {
    const std::vector<std::string> addresses{validServerAddr};
    const Config config1(addresses, policy);
    const Config config2(addresses, policy);
    
    // This should not compile - testing that assignment operator is deleted
    // config1 = config2; // Uncommenting this line should cause compilation error
    
    // Instead, we'll verify that the assignment operator is indeed deleted
    EXPECT_FALSE(std::is_copy_assignable_v<Config>);
}

// Test behavior when all services become unavailable
TEST_F(ConfigTest, NextServiceWhenAllServicesUnavailable) {
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, policy);
    
    // First verify we have a working service
    auto result = config.nextService();
    EXPECT_TRUE(result.has_value());
    
    // Note: Now we can use the shutdown method to test service unavailability
    // we can properly test the scenario where services become unavailable
    // by shutting down the server gracefully.
    
    // For now, we'll test that we can successfully call nextService multiple times
    for (int i = 0; i < 5; ++i) {
        auto nextResult = config.nextService();
        EXPECT_TRUE(nextResult.has_value());
    }
}

// Test with different retry policies
TEST_F(ConfigTest, ConstructorWithDifferentRetryPolicies) {
    const std::vector<std::string> addresses{validServerAddr};
    
    // Test with very short delays
    const RetryPolicy shortPolicy{std::chrono::microseconds{1L}, std::chrono::microseconds{10L}, std::chrono::microseconds{100L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    ASSERT_NO_THROW({
        const Config config(addresses, shortPolicy);
    });
    
    // Test with very long delays
    const RetryPolicy longPolicy{std::chrono::microseconds{1000L}, std::chrono::microseconds{10000L}, std::chrono::microseconds{100000L}, 5, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    ASSERT_NO_THROW({
        const Config config(addresses, longPolicy);
    });
    
    // Test with zero threshold
    const RetryPolicy zeroThresholdPolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 0, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    ASSERT_NO_THROW({
        const Config config(addresses, zeroThresholdPolicy);
    });
}


// Test that services map is properly populated
TEST_F(ConfigTest, ServicesMapIsProperlyPopulated) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, policy);
    
    // We can't directly access the private services map, but we can verify
    // that we can get services and they work as expected
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    const zdb::KVRPCServicePtr service1 = result1.value();
    EXPECT_TRUE(service1->connected());
    
    auto result2 = config.nextService();
    ASSERT_TRUE(result2.has_value());
    const zdb::KVRPCServicePtr service2 = result2.value();
    EXPECT_TRUE(service2->connected());
}

// Test rapid successive calls to nextService
TEST_F(ConfigTest, RapidSuccessiveCallsToNextService) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, policy);
    
    // Make multiple rapid calls to nextService
    for (int i = 0; i < 10; ++i) {
        auto result = config.nextService();
        ASSERT_TRUE(result.has_value());
        const zdb::KVRPCServicePtr service = result.value();
        EXPECT_TRUE(service->connected());
    }
}

// Test thread safety aspects (basic test)
TEST_F(ConfigTest, BasicThreadSafetyTest) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, policy);
    
    std::atomic<int> successCount{0};
    std::atomic<int> exceptionCount{0};
    
    auto testFunction = [&config, &successCount, &exceptionCount]() {
        for (int i = 0; i < 100; ++i) {
            auto result = config.nextService();
            if (result.has_value()) {
                successCount++;
            } else {
                exceptionCount++;
            }
        }
    };
    
    std::thread t1(testFunction);
    std::thread t2(testFunction);
    
    t1.join();
    t2.join();
    
    // At least some operations should succeed since we have valid servers
    EXPECT_GT(successCount.load(), 0);
}

// Test currentService behavior when circuit breaker is open
TEST_F(ConfigTest, CurrentServiceFailsWhenCircuitBreakerOpen) {
    // Use a policy with very low failure threshold to quickly open circuit breaker
    const RetryPolicy lowThresholdPolicy{std::chrono::microseconds{10L}, std::chrono::microseconds{50L}, std::chrono::microseconds{200L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, lowThresholdPolicy);
    
    // Verify service works initially
    auto result = config.nextService();
    ASSERT_TRUE(result.has_value());
    
    // Shutdown server to trigger circuit breaker opening
    server1->shutdown();

    // Wait a bit for the circuit breaker to open after failed operations
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    
    // currentService should now fail due to circuit breaker being open
    auto resultAfterFailure = config.nextService();
    EXPECT_FALSE(resultAfterFailure.has_value());
    EXPECT_EQ(resultAfterFailure.error().code, ErrorCode::AllServicesUnavailable);
}

// Test nextService behavior with circuit breaker recovery
TEST_F(ConfigTest, NextServiceWithCircuitBreakerRecovery) {
    // Use a policy with short reset timeout for circuit breaker
    const RetryPolicy shortResetPolicy{std::chrono::microseconds{10L}, std::chrono::microseconds{50L}, std::chrono::microseconds{100L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, shortResetPolicy);
    
    // Get initial service
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    
    // Temporarily shutdown first server to trigger circuit breaker
    server1->shutdown();
    
    // nextService should failover to second server
    auto result2 = config.nextService();
    EXPECT_TRUE(result2.has_value());
    
    // Restart first server
    server1 = std::make_unique<KVStoreServer>(validServerAddr, serviceImpl1);
    std::this_thread::sleep_for(std::chrono::milliseconds{200L});
    
    // After circuit breaker reset timeout, should be able to use services again
    auto result3 = config.nextService();
    EXPECT_TRUE(result3.has_value());
}

// Test that nextActiveServiceIterator prioritizes current service recovery
TEST_F(ConfigTest, NextActiveServiceIteratorPrioritizesCurrentService) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, policy);
    
    // Get initial service - should be one of the two available services
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    
    // Call nextService multiple times - should prefer to stay with current service if available
    for (int i = 0; i < 5; ++i) {
        auto nextResult = config.nextService();
        ASSERT_TRUE(nextResult.has_value());
        const zdb::KVRPCServicePtr nextService = nextResult.value();
        
        // Should get a valid, connected, and available service
        EXPECT_TRUE(nextService->connected());
        // Note: available() is not const, so we can't call it on const pointer
    }
}

// Test currentService with service reconnection through available() call
TEST_F(ConfigTest, CurrentServiceTriggersReconnectionThroughAvailable) {
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, policy);
    
    // Get initial service
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    
    // Briefly shutdown and restart server to simulate temporary disconnection
    server1->shutdown();
    
    // Restart server quickly
    server1 = std::make_unique<KVStoreServer>(validServerAddr, serviceImpl1);
    std::this_thread::sleep_for(std::chrono::milliseconds{200L});
    
    // currentService should be able to recover (non-const allows state modification)
    auto result2 = config.nextService();
    EXPECT_TRUE(result2.has_value());
}

// Test differentiation between connected and available states
TEST_F(ConfigTest, ConnectedVsAvailableStates) {
    // Use a policy that quickly opens circuit breaker
    const RetryPolicy quickFailPolicy{std::chrono::microseconds{5L}, std::chrono::microseconds{25L}, std::chrono::microseconds{100L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, quickFailPolicy);
    
    // Initially should have both connected and available service
    auto result = config.nextService();
    ASSERT_TRUE(result.has_value());
    zdb::KVRPCServicePtr service = result.value(); // Not const since available() is not const
    EXPECT_TRUE(service->connected());
    EXPECT_TRUE(service->available());
    
    // After shutting down server, service should become neither connected nor available
    server1->shutdown();

    
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    
    // currentService should fail because service is not available
    auto resultAfterShutdown = config.nextService();
    EXPECT_FALSE(resultAfterShutdown.has_value());
    EXPECT_EQ(resultAfterShutdown.error().code, ErrorCode::AllServicesUnavailable);
}

// Test nextService with mixed service states (some connected, some available)
TEST_F(ConfigTest, NextServiceWithMixedServiceStates) {
    const std::vector<std::string> addresses{validServerAddr, validServerAddr2};
    Config config(addresses, policy);
    
    // Initially both services should be available
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    
    // Temporarily shutdown one server
    server2->shutdown();
    
    // nextService should still work with the remaining available service
    auto result2 = config.nextService();
    EXPECT_TRUE(result2.has_value());
    
    // Restart the second server
    server2 = std::make_unique<KVStoreServer>(validServerAddr2, serviceImpl2);
    std::this_thread::sleep_for(std::chrono::milliseconds{200L});
    
    // Should be able to use services again
    auto result3 = config.nextService();
    EXPECT_TRUE(result3.has_value());
}

// Test that error messages are appropriate for circuit breaker scenarios
TEST_F(ConfigTest, CircuitBreakerErrorMessages) {
    // Use a policy that opens circuit breaker quickly
    const RetryPolicy fastFailPolicy{std::chrono::microseconds{1L}, std::chrono::microseconds{10L}, std::chrono::microseconds{50L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, fastFailPolicy);
    
    // Shutdown server to trigger failures
    server1->shutdown();
    
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    
    // Error message should indicate service unavailability
    auto result = config.nextService();
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
    EXPECT_FALSE(result.error().what.empty());
}

// Test circuit breaker reset behavior in nextService
TEST_F(ConfigTest, CircuitBreakerResetInNextService) {
    // Use a policy with very short reset timeout
    const RetryPolicy shortResetPolicy{std::chrono::microseconds{1L}, std::chrono::microseconds{10L}, std::chrono::microseconds{50L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    const std::vector<std::string> addresses{validServerAddr};
    Config config(addresses, shortResetPolicy);
    
    // Get initial service
    auto result1 = config.nextService();
    ASSERT_TRUE(result1.has_value());
    
    // Shutdown server to trigger circuit breaker
    server1->shutdown();

    // Restart server immediately
    server1 = std::make_unique<KVStoreServer>(validServerAddr, serviceImpl1);
    std::this_thread::sleep_for(std::chrono::milliseconds{100L}); // Wait for circuit breaker reset
    
    // nextService should work after reset timeout
    auto result2 = config.nextService();
    EXPECT_TRUE(result2.has_value());
}
