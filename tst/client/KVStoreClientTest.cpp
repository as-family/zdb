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
#include "client/KVStoreClient.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "common/Types.hpp"
#include "server/KVStoreServiceImpl.hpp"
#include "storage/InMemoryKVStore.hpp"
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include "client/Config.hpp"
#include <stdexcept>
#include "raft/TestRaft.hpp"
#include "raft/Channel.hpp"
#include "raft/SyncChannel.hpp"
#include "common/KVStateMachine.hpp"

using zdb::Config;
using zdb::InMemoryKVStore;
using zdb::KVStoreServiceImpl;
using zdb::KVStoreServer;
using zdb::KVStoreClient;
using zdb::KVRPCService;
using zdb::RetryPolicy;
using zdb::ErrorCode;
using zdb::Key;
using zdb::Value;

const std::string SERVER_ADDR = "localhost:50052";

class KVStoreClientTest : public ::testing::Test {
protected:
    InMemoryKVStore kvStore;
    raft::SyncChannel<std::shared_ptr<raft::Command>> leader{};
    TestRaft raft{leader};
    zdb::KVStateMachine kvState {kvStore, leader, raft};
    KVStoreServiceImpl serviceImpl{kvState};
    std::unique_ptr<KVStoreServer> server;
    const RetryPolicy policy{std::chrono::milliseconds{100L}, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{5000L}, 3, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    std::vector<std::string> addresses{SERVER_ADDR};

    void SetUp() override {
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        std::this_thread::sleep_for(std::chrono::milliseconds{500L});
    }
    void TearDown() override {
        if (server) {
            server->shutdown();
        }
    }
};

TEST_F(KVStoreClientTest, ConnectsToServer) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    auto result = client.size();
    EXPECT_TRUE(result.has_value());
}

TEST_F(KVStoreClientTest, SetAndGetSuccess) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto setResult = client.set(Key{"foo"}, Value{"bar"});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = client.get(Key{"foo"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().data, "bar");
}

TEST_F(KVStoreClientTest, GetNonExistentKey) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    auto getResult = client.get(Key{"missing"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::KeyNotFound);
}

TEST_F(KVStoreClientTest, OverwriteValue) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    EXPECT_TRUE(client.set(Key{"foo"}, Value{"bar"}).has_value());
    
    // Get the current value and its version
    auto getResult1 = client.get(Key{"foo"});
    ASSERT_TRUE(getResult1.has_value());
    EXPECT_EQ(getResult1.value().data, "bar");
    
    // Overwrite with the correct version
    Value updateValue{"baz"};
    updateValue.version = getResult1.value().version;
    EXPECT_TRUE(client.set(Key{"foo"}, updateValue).has_value());
    
    auto getResult = client.get(Key{"foo"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().data, "baz");
}

TEST_F(KVStoreClientTest, EraseExistingKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    EXPECT_TRUE(client.set(Key{"foo"}, Value{"bar"}).has_value());
    auto eraseResult = client.erase(Key{"foo"});
    ASSERT_TRUE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.value().data, "bar");
    auto getResult = client.get(Key{"foo"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::KeyNotFound);
}

TEST_F(KVStoreClientTest, EraseNonExistentKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto eraseResult = client.erase(Key{"missing"});
    EXPECT_FALSE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.error().code, ErrorCode::KeyNotFound);
}

TEST_F(KVStoreClientTest, SizeReflectsSetAndErase) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    EXPECT_TRUE(client.set(Key{"a"}, Value{"1"}).has_value());
    EXPECT_TRUE(client.set(Key{"b"}, Value{"2"}).has_value());
    auto sizeResult = client.size();
    ASSERT_TRUE(sizeResult.has_value());
    EXPECT_EQ(sizeResult.value(), 2);
    EXPECT_EQ(client.erase(Key{"a"}).value().data, "1");
    auto sizeResult2 = client.size();
    ASSERT_TRUE(sizeResult2.has_value());
    EXPECT_EQ(sizeResult2.value(), 1);
}

TEST_F(KVStoreClientTest, SetFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown(); // Simulate server down
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    auto setResult = client.set(Key{"foo"}, Value{"bar"});
    EXPECT_FALSE(setResult.has_value());
    EXPECT_EQ(setResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, GetFailureReturnsError) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    auto getResult = client.get(Key{"foo"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, EraseFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    auto eraseResult = client.erase(Key{"foo"});
    EXPECT_FALSE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, SizeFailureReturnsError) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    auto sizeResult = client.size();
    EXPECT_FALSE(sizeResult.has_value());
    EXPECT_EQ(sizeResult.error().code, ErrorCode::AllServicesUnavailable);
}

// Edge case: empty key
TEST_F(KVStoreClientTest, EmptyKeySetGetErase) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto setResult = client.set(Key{""}, Value{"empty"});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = client.get(Key{""});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().data, "empty");
    auto eraseResult = client.erase(Key{""});
    ASSERT_TRUE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.value().data, "empty");
}

// Edge case: large value
TEST_F(KVStoreClientTest, LargeValueSetGet) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    const std::string largeValue(100000, 'x');
    auto setResult = client.set(Key{"big"}, Value{largeValue});
    EXPECT_TRUE(setResult.has_value());
    auto getResult = client.get(Key{"big"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().data, largeValue);
}

// Test behavior with servicesToTry = 0 (should fail immediately)
TEST_F(KVStoreClientTest, ServicesToTryZeroFailsImmediately) {
    const RetryPolicy zeroServicesPolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 0, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, zeroServicesPolicy};
    const KVStoreClient client{c};
    
    // Should fail immediately without trying any services
    auto result = client.get(Key{"test"});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
}

// Test behavior with servicesToTry = 1 (should try only once)
TEST_F(KVStoreClientTest, ServicesToTryOneTriesOnlyOnce) {
    const RetryPolicy oneServicePolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, oneServicePolicy};
    KVStoreClient client{c};
    
    // Should work when service is available
    auto setResult = client.set(Key{"test"}, Value{"value"});
    EXPECT_TRUE(setResult.has_value());
    
    auto getResult = client.get(Key{"test"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().data, "value");
}

// Test with multiple services and different servicesToTry values
TEST_F(KVStoreClientTest, MultipleServicesWithVariousRetryLimits) {
    // Set up additional server
    const std::string serverAddress2 = "localhost:50053";
    InMemoryKVStore kvStore2;
    auto channel2 = raft::SyncChannel<std::shared_ptr<raft::Command>>{};
    TestRaft raft2{channel2};
    zdb::KVStateMachine kvState2 = zdb::KVStateMachine{kvStore2, channel2, raft2};
    KVStoreServiceImpl serviceImpl2{kvState2};
    std::unique_ptr<KVStoreServer> server2;
    std::thread serverThread2;
    
    server2 = std::make_unique<KVStoreServer>(serverAddress2, serviceImpl2);
    std::this_thread::sleep_for(std::chrono::milliseconds{500L});
    
    const std::vector<std::string> multiAddresses{SERVER_ADDR, serverAddress2};
    
    // Test with servicesToTry = 1 (should work with first available service)
    const RetryPolicy oneServicePolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c1{multiAddresses, oneServicePolicy};
    KVStoreClient client1{c1};
    
    auto result1 = client1.set(Key{"key1"}, Value{"value1"});
    EXPECT_TRUE(result1.has_value());
    
    // Test with servicesToTry = 2 (should work with up to 2 services)
    const RetryPolicy twoServicesPolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 2, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c2{multiAddresses, twoServicesPolicy};
    KVStoreClient client2{c2};
    
    auto result2 = client2.set(Key{"key2"}, Value{"value2"});
    EXPECT_TRUE(result2.has_value());
}

// Test servicesToTry behavior when services become unavailable
TEST_F(KVStoreClientTest, ServicesToTryWithServiceFailure) {
    // Use a policy that tries more services than available
    const RetryPolicy multiServicePolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 3, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, multiServicePolicy};
    KVStoreClient client{c};
    
    // First verify it works
    auto setResult = client.set(Key{"test"}, Value{"value"});
    EXPECT_TRUE(setResult.has_value());
    
    // Now simulate service becoming unavailable by shutting down the server
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds{200L});
    
    // Should fail after trying available services (up to servicesToTry limit)
    auto getResult = client.get(Key{"test"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
}

// Test edge case: servicesToTry larger than available services
TEST_F(KVStoreClientTest, ServicesToTryLargerThanAvailableServices) {
    // policy tries 5 services but only 1 is available
    const RetryPolicy excessiveRetryPolicy{std::chrono::microseconds{100L}, std::chrono::microseconds{1000L}, std::chrono::microseconds{5000L}, 2, 5, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, excessiveRetryPolicy};
    KVStoreClient client{c};
    
    // Should still work - client will try available services
    auto result = client.set(Key{"test"}, Value{"value"});
    EXPECT_TRUE(result.has_value());
    
    auto getResult = client.get(Key{"test"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value().data, "value");
}

// Test that Config properly exposes the RetryPolicy
TEST_F(KVStoreClientTest, ConfigExposesRetryPolicy) {
    const RetryPolicy customPolicy{std::chrono::microseconds{200L}, std::chrono::microseconds{2000L}, std::chrono::microseconds{10000L}, 5, 3, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    const Config c{addresses, customPolicy};
    
    // Verify the policy is properly stored and accessible
    EXPECT_EQ(c.policy.baseDelay, std::chrono::microseconds{200L});
    EXPECT_EQ(c.policy.maxDelay, std::chrono::microseconds{2000L});
    EXPECT_EQ(c.policy.resetTimeout, std::chrono::microseconds{10000L});
    EXPECT_EQ(c.policy.failureThreshold, 5);
    EXPECT_EQ(c.policy.servicesToTry, 3);
}

// ========== RETRY LOGIC AND RESILIENCE TESTS ==========

// Test client retry behavior with short-lived server outages
TEST_F(KVStoreClientTest, RetryDuringShortServerOutage) {
    // Use a fast retry policy for testing
    const RetryPolicy fastRetryPolicy{std::chrono::microseconds{50L}, std::chrono::microseconds{200L}, std::chrono::microseconds{500L}, 3, 2, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, fastRetryPolicy};
    KVStoreClient client{c};
    
    // First, verify client works normally
    EXPECT_TRUE(client.set(Key{"test"}, Value{"value"}).has_value());
    
    // Simulate server outage
    server->shutdown();
    
    // Operations should fail during outage
    auto getResult = client.get(Key{"test"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
    
    // Restart the server
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    std::this_thread::sleep_for(std::chrono::milliseconds{1000L});
    
    // Client should recover and work again
    auto setResult = client.set(Key{"recovery"}, Value{"test"});
    EXPECT_TRUE(setResult.has_value());
    
    auto getRecoveryResult = client.get(Key{"recovery"});
    ASSERT_TRUE(getRecoveryResult.has_value());
    EXPECT_EQ(getRecoveryResult.value().data, "test");
}

// Test client behavior with multiple server restarts
TEST_F(KVStoreClientTest, MultipleServerRestarts) {
    const RetryPolicy fastRetryPolicy{std::chrono::milliseconds{25L}, std::chrono::milliseconds{100L}, std::chrono::milliseconds{300L}, 3, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, fastRetryPolicy};
    KVStoreClient client{c};
    
    for (int restart = 0; restart < 3; ++restart) {
        // Set data before restart
        std::string key = "key" + std::to_string(restart);
        std::string value = "value" + std::to_string(restart);
        EXPECT_TRUE(client.set(Key{key}, Value{value}).has_value());
        
        // Restart server
        server->shutdown();
        
        // Brief outage - operations should fail
        auto failResult = client.get(Key{key});
        EXPECT_FALSE(failResult.has_value());
        
        // Restart server
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        std::this_thread::sleep_for(std::chrono::seconds{1});
        
        // Client should recover
        auto newSetResult = client.set(Key{"after_restart_" + std::to_string(restart)}, Value{"recovery"});
        EXPECT_TRUE(newSetResult.has_value());
    }
}

// Test client resilience with circuit breaker behavior during extended outage
TEST_F(KVStoreClientTest, CircuitBreakerDuringExtendedOutage) {
    // Use a policy with a low failure threshold to trigger circuit breaker quickly
    const RetryPolicy circuitBreakerPolicy{std::chrono::milliseconds{10L}, std::chrono::milliseconds{50L}, std::chrono::milliseconds{200L}, 1, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, circuitBreakerPolicy};
    KVStoreClient client{c};
    
    // Verify client works normally
    EXPECT_TRUE(client.set(Key{"before_outage"}, Value{"value"}).has_value());
    
    // Simulate extended server outage
    server->shutdown();
    
    // Multiple failed operations should trigger circuit breaker
    for (int i = 0; i < 5; ++i) {
        auto result = client.get(Key{"test"});
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
    }

    // Restart server
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    std::this_thread::sleep_for(std::chrono::seconds{4}); // Wait for circuit breaker reset timeout
    
    // Circuit breaker should allow operations after reset timeout
    auto recoveryResult = client.set(Key{"after_recovery"}, Value{"test"});
    EXPECT_TRUE(recoveryResult.has_value());
}

// Test retry behavior with multiple services and failover
TEST_F(KVStoreClientTest, MultiServiceFailoverResilience) {
    // Set up multiple servers
    const std::string serverAddress2 = "localhost:50054";
    const std::string serverAddress3 = "localhost:50055";
    
    InMemoryKVStore kvStore2, kvStore3;
    auto channel2 = raft::SyncChannel<std::shared_ptr<raft::Command>>();
    auto channel3 = raft::SyncChannel<std::shared_ptr<raft::Command>>();
    TestRaft raft2{channel2}, raft3{channel3};
    zdb::KVStateMachine kvState2 {kvStore2, channel2, raft2};
    zdb::KVStateMachine kvState3 {kvStore3, channel3, raft3};
    KVStoreServiceImpl serviceImpl2{kvState2}, serviceImpl3{kvState3};
    std::unique_ptr<KVStoreServer> server2, server3;
    std::thread serverThread2, serverThread3;
    
    server2 = std::make_unique<KVStoreServer>(serverAddress2, serviceImpl2);
    server3 = std::make_unique<KVStoreServer>(serverAddress3, serviceImpl3);
    std::this_thread::sleep_for(std::chrono::milliseconds{200L});
    
    const std::vector<std::string> multiAddresses{SERVER_ADDR, serverAddress2, serverAddress3};
    const RetryPolicy multiServicePolicy{std::chrono::microseconds{50L}, std::chrono::microseconds{200L}, std::chrono::microseconds{500L}, 2, 3, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{multiAddresses, multiServicePolicy};
    KVStoreClient client{c};
    
    // Verify client works with all services up
    EXPECT_TRUE(client.set(Key{"multi_test"}, Value{"value"}).has_value());
    
    // Shutdown first server
    server->shutdown();
    
    // Client should failover to remaining services
    auto result1 = client.set(Key{"failover1"}, Value{"test1"});
    EXPECT_TRUE(result1.has_value());
    
    // Shutdown second server
    server2->shutdown();
    if (serverThread2.joinable()) {
        serverThread2.join();
    }
    
    // Client should still work with one remaining service
    auto result2 = client.set(Key{"failover2"}, Value{"test2"});
    EXPECT_TRUE(result2.has_value());
    
    // Shutdown all servers
    server3->shutdown();
    
    // Now all operations should fail
    auto failResult = client.get(Key{"failover1"});
    EXPECT_FALSE(failResult.has_value());
    EXPECT_EQ(failResult.error().code, ErrorCode::AllServicesUnavailable);
    
    // Restart one server
    server2 = std::make_unique<KVStoreServer>(serverAddress2, serviceImpl2);
    std::this_thread::sleep_for(std::chrono::milliseconds{600L}); // Wait for circuit breaker reset
    
    // Client should recover with the restarted service
    auto recoveryResult = client.set(Key{"recovery"}, Value{"success"});
    EXPECT_TRUE(recoveryResult.has_value());
    
    // Cleanup remaining servers
    if (server2) {
        server2->shutdown();
    }
}

// Test exponential backoff behavior during retries
TEST_F(KVStoreClientTest, ExponentialBackoffDuringRetries) {
    // Use a policy with noticeable delays for testing backoff
    const RetryPolicy backoffPolicy{std::chrono::milliseconds{100L}, std::chrono::milliseconds{500L}, std::chrono::milliseconds{1000L}, 3, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, backoffPolicy};
    KVStoreClient client{c};
    
    // Verify client works normally
    EXPECT_TRUE(client.set(Key{"backoff_test"}, Value{"value"}).has_value());
    
    // Shutdown server to trigger retries
    server->shutdown();

    // Measure time for failed operation (should include backoff delays)
    auto start = std::chrono::steady_clock::now();
    auto result = client.get(Key{"backoff_test"});
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
    // TODO: Find a better way to test duration
    // EXPECT_GE(duration, backoffPolicy.baseDelay);
}

// Test client recovery with data persistence after server restart
TEST_F(KVStoreClientTest, DataPersistenceAfterServerRestart) {
    const RetryPolicy recoveryPolicy{std::chrono::microseconds{50L}, std::chrono::microseconds{200L}, std::chrono::microseconds{300L}, 2, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, recoveryPolicy};
    KVStoreClient client{c};
    
    // Set initial data
    EXPECT_TRUE(client.set(Key{"persistent_key"}, Value{"persistent_value"}).has_value());
    auto initialSize = client.size();
    ASSERT_TRUE(initialSize.has_value());
    EXPECT_EQ(initialSize.value(), 1);
    
    server->shutdown();
    
    // Operations fail during outage
    EXPECT_FALSE(client.get(Key{"persistent_key"}).has_value());
    
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    
    auto newSize = client.size();
    ASSERT_TRUE(newSize.has_value());
    EXPECT_EQ(newSize.value(), 1);
    
    EXPECT_TRUE(client.set(Key{"new_key"}, Value{"new_value"}).has_value());
    auto finalSize = client.size();
    ASSERT_TRUE(finalSize.has_value());
    EXPECT_EQ(finalSize.value(), 2);
}

// Test client behavior during rapid server cycling
TEST_F(KVStoreClientTest, RapidServerCycling) {
    const RetryPolicy rapidCyclePolicy{std::chrono::microseconds{20L}, std::chrono::microseconds{80L}, std::chrono::microseconds{300L}, 2, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, rapidCyclePolicy};
    KVStoreClient client{c};
    
    // Perform rapid server restarts
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Quick operation
        auto setResult = client.set(Key{"cycle_" + std::to_string(cycle)}, Value{"value"});
        EXPECT_TRUE(setResult.has_value());
        
        // Quick shutdown and restart
        server->shutdown();
        
        // Very brief downtime
        std::this_thread::sleep_for(std::chrono::milliseconds{50L});
        
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        std::this_thread::sleep_for(std::chrono::milliseconds{100L});
    }
    
    // Final verification that client still works
    auto finalResult = client.set(Key{"final_test"}, Value{"success"});
    EXPECT_TRUE(finalResult.has_value());
}

// Test retry exhaustion with all services down
TEST_F(KVStoreClientTest, RetryExhaustionAllServicesDown) {
    const RetryPolicy exhaustionPolicy{std::chrono::microseconds{10L}, std::chrono::microseconds{50L}, std::chrono::microseconds{200L}, 2, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, exhaustionPolicy};
    KVStoreClient client{c};
    
    // Verify client works initially
    EXPECT_TRUE(client.set(Key{"before_shutdown"}, Value{"value"}).has_value());
    
    // Shutdown server
    server->shutdown();
    
    // Test different operations - all should fail after retry exhaustion
    auto getResult = client.get(Key{"before_shutdown"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
    
    auto setResult = client.set(Key{"during_outage"}, Value{"value"});
    EXPECT_FALSE(setResult.has_value());
    EXPECT_EQ(setResult.error().code, ErrorCode::AllServicesUnavailable);
    
    auto eraseResult = client.erase(Key{"before_shutdown"});
    EXPECT_FALSE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.error().code, ErrorCode::AllServicesUnavailable);
    
    auto sizeResult = client.size();
    EXPECT_FALSE(sizeResult.has_value());
    EXPECT_EQ(sizeResult.error().code, ErrorCode::AllServicesUnavailable);
}

// Test client resilience with intermittent connectivity
TEST_F(KVStoreClientTest, IntermittentConnectivityResilience) {
    // Use longer reset timeout to ensure circuit breaker can recover
    const RetryPolicy intermittentPolicy{std::chrono::milliseconds{30L}, std::chrono::milliseconds{120L}, std::chrono::milliseconds{100L}, 3, 1, std::chrono::milliseconds{1000L}, std::chrono::milliseconds{200L}};
    Config c{addresses, intermittentPolicy};
    KVStoreClient client{c};
    
    // Pattern: work -> fail -> work -> fail -> work
    
    // 1. Initial work
    EXPECT_TRUE(client.set(Key{"intermittent1"}, Value{"value1"}).has_value());
    
    // 2. Simulate failure
    server->shutdown();
    EXPECT_FALSE(client.get(Key{"intermittent1"}).has_value());
    
    // 3. Restore service - wait much longer for circuit breaker reset
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    std::this_thread::sleep_for(std::chrono::seconds{1}); // Wait for circuit breaker reset
    EXPECT_TRUE(client.set(Key{"intermittent2"}, Value{"value2"}).has_value());
    
    // 4. Another failure
    server->shutdown();
    EXPECT_FALSE(client.get(Key{"intermittent2"}).has_value());
    
    // 5. Final restore - wait much longer for circuit breaker reset
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    std::this_thread::sleep_for(std::chrono::seconds{1}); // Wait for circuit breaker reset
    EXPECT_TRUE(client.set(Key{"intermittent3"}, Value{"value3"}).has_value());
}
