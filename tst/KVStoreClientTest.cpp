#include <gtest/gtest.h>
#include "client/KVStoreClient.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "common/Types.hpp"
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include "client/Config.hpp"
#include <stdexcept>
#include "client/KVRPCService.hpp"

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
    KVStoreServiceImpl serviceImpl{kvStore};
    std::unique_ptr<KVStoreServer> server;
    std::thread serverThread;
    const RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
    std::vector<std::string> addresses{SERVER_ADDR};

    void SetUp() override {
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        serverThread = std::thread([this]() { server->wait(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    void TearDown() override {
        if (server) {
            server->shutdown();
        }
        if (serverThread.joinable()) {
            serverThread.join();
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
    EXPECT_EQ(getResult.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, OverwriteValue) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    EXPECT_TRUE(client.set(Key{"foo"}, Value{"bar"}).has_value());
    EXPECT_TRUE(client.set(Key{"foo"}, Value{"baz"}).has_value());
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
    EXPECT_EQ(getResult.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, EraseNonExistentKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto eraseResult = client.erase(Key{"missing"});
    EXPECT_FALSE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, SizeReflectsSetAndErase) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    EXPECT_TRUE(client.set(Key{"a"}, Value{"1"}).has_value());
    EXPECT_TRUE(client.set(Key{"b"}, Value{"2"}).has_value());
    auto sizeResult = client.size();
    ASSERT_TRUE(sizeResult.has_value());
    EXPECT_EQ(sizeResult.value(), 2);
    EXPECT_EQ(client.erase(Key{"a"}), Value{"1"});
    auto sizeResult2 = client.size();
    ASSERT_TRUE(sizeResult2.has_value());
    EXPECT_EQ(sizeResult2.value(), 1);
}

TEST_F(KVStoreClientTest, FailureToConnectThrows) {
    const std::vector<std::string> badAddresses{"localhost:59999"};
    EXPECT_THROW({
        Config c(badAddresses, policy);
        const KVStoreClient client(c);
    }, std::runtime_error);
}

TEST_F(KVStoreClientTest, SetFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown(); // Simulate server down
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto setResult = client.set(Key{"foo"}, Value{"bar"});
    EXPECT_FALSE(setResult.has_value());
    EXPECT_EQ(setResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, GetFailureReturnsError) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto getResult = client.get(Key{"foo"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, EraseFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto eraseResult = client.erase(Key{"foo"});
    EXPECT_FALSE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, SizeFailureReturnsError) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
    EXPECT_EQ(getResult.value(), Value{largeValue});
}

// Test behavior with servicesToTry = 0 (should fail immediately)
TEST_F(KVStoreClientTest, ServicesToTryZeroFailsImmediately) {
    const RetryPolicy zeroServicesPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 0};
    Config c{addresses, zeroServicesPolicy};
    const KVStoreClient client{c};
    
    // Should fail immediately without trying any services
    auto result = client.get(Key{"test"});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
}

// Test behavior with servicesToTry = 1 (should try only once)
TEST_F(KVStoreClientTest, ServicesToTryOneTriesOnlyOnce) {
    const RetryPolicy oneServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 1};
    Config c{addresses, oneServicePolicy};
    KVStoreClient client{c};
    
    // Should work when service is available
    auto setResult = client.set(Key{"test"}, Value{"value"});
    EXPECT_TRUE(setResult.has_value());
    
    auto getResult = client.get(Key{"test"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), Value{"value"});
}

// Test with multiple services and different servicesToTry values
TEST_F(KVStoreClientTest, MultipleServicesWithVariousRetryLimits) {
    // Set up additional server
    const std::string serverAddress2 = "localhost:50053";
    InMemoryKVStore kvStore2;
    KVStoreServiceImpl serviceImpl2{kvStore2};
    std::unique_ptr<KVStoreServer> server2;
    std::thread serverThread2;
    
    server2 = std::make_unique<KVStoreServer>(serverAddress2, serviceImpl2);
    serverThread2 = std::thread([&server2]() { server2->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    const std::vector<std::string> multiAddresses{SERVER_ADDR, serverAddress2};
    
    // Test with servicesToTry = 1 (should work with first available service)
    const RetryPolicy oneServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 1};
    Config c1{multiAddresses, oneServicePolicy};
    KVStoreClient client1{c1};
    
    auto result1 = client1.set(Key{"key1"}, Value{"value1"});
    EXPECT_TRUE(result1.has_value());
    
    // Test with servicesToTry = 2 (should work with up to 2 services)
    const RetryPolicy twoServicesPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
    Config c2{multiAddresses, twoServicesPolicy};
    KVStoreClient client2{c2};
    
    auto result2 = client2.set(Key{"key2"}, Value{"value2"});
    EXPECT_TRUE(result2.has_value());
    
    // Cleanup second server
    if (server2) {
        server2->shutdown();
    }
    if (serverThread2.joinable()) {
        serverThread2.join();
    }
}

// Test servicesToTry behavior when services become unavailable
TEST_F(KVStoreClientTest, ServicesToTryWithServiceFailure) {
    // Use a policy that tries more services than available
    const RetryPolicy multiServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 3};
    Config c{addresses, multiServicePolicy};
    KVStoreClient client{c};
    
    // First verify it works
    auto setResult = client.set(Key{"test"}, Value{"value"});
    EXPECT_TRUE(setResult.has_value());
    
    // Now simulate service becoming unavailable by shutting down the server
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should fail after trying available services (up to servicesToTry limit)
    auto getResult = client.get(Key{"test"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
}

// Test edge case: servicesToTry larger than available services
TEST_F(KVStoreClientTest, ServicesToTryLargerThanAvailableServices) {
    // policy tries 5 services but only 1 is available
    const RetryPolicy excessiveRetryPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 5};
    Config c{addresses, excessiveRetryPolicy};
    KVStoreClient client{c};
    
    // Should still work - client will try available services
    auto result = client.set(Key{"test"}, Value{"value"});
    EXPECT_TRUE(result.has_value());
    
    auto getResult = client.get(Key{"test"});
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), Value{"value"});
}

// Test that Config properly exposes the RetryPolicy
TEST_F(KVStoreClientTest, ConfigExposesRetryPolicy) {
    const RetryPolicy customPolicy{std::chrono::microseconds(200), std::chrono::microseconds(2000), std::chrono::microseconds(10000), 5, 3};
    const Config c{addresses, customPolicy};
    
    // Verify the policy is properly stored and accessible
    EXPECT_EQ(c.policy.baseDelay, std::chrono::microseconds(200));
    EXPECT_EQ(c.policy.maxDelay, std::chrono::microseconds(2000));
    EXPECT_EQ(c.policy.resetTimeout, std::chrono::microseconds(10000));
    EXPECT_EQ(c.policy.failureThreshold, 5);
    EXPECT_EQ(c.policy.servicesToTry, 3);
}

// ========== RETRY LOGIC AND RESILIENCE TESTS ==========

// Test client retry behavior with short-lived server outages
TEST_F(KVStoreClientTest, RetryDuringShortServerOutage) {
    // Use a fast retry policy for testing
    const RetryPolicy fastRetryPolicy{std::chrono::microseconds(50), std::chrono::microseconds(200), std::chrono::microseconds(500), 3, 2};
    Config c{addresses, fastRetryPolicy};
    KVStoreClient client{c};
    
    // First, verify client works normally
    EXPECT_TRUE(client.set(Key{"test"}, Value{"value"}).has_value());
    
    // Simulate server outage
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    // Operations should fail during outage
    auto getResult = client.get(Key{"test"});
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
    
    // Restart the server
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    serverThread = std::thread([this]() { server->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(600)); // Wait for circuit breaker reset
    
    // Client should recover and work again
    auto setResult = client.set(Key{"recovery"}, Value{"test"});
    EXPECT_TRUE(setResult.has_value());
    
    auto getRecoveryResult = client.get(Key{"recovery"});
    ASSERT_TRUE(getRecoveryResult.has_value());
    EXPECT_EQ(getRecoveryResult.value(), Value{"test"});
}

// Test client behavior with multiple server restarts
TEST_F(KVStoreClientTest, MultipleServerRestarts) {
    const RetryPolicy fastRetryPolicy{std::chrono::microseconds(25), std::chrono::microseconds(100), std::chrono::microseconds(300), 2, 1};
    Config c{addresses, fastRetryPolicy};
    KVStoreClient client{c};
    
    for (int restart = 0; restart < 3; ++restart) {
        // Set data before restart
        std::string key = "key" + std::to_string(restart);
        std::string value = "value" + std::to_string(restart);
        EXPECT_TRUE(client.set(Key{key}, Value{value}).has_value());
        
        // Restart server
        server->shutdown();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        
        // Brief outage - operations should fail
        auto failResult = client.get(Key{key});
        EXPECT_FALSE(failResult.has_value());
        
        // Restart server
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        serverThread = std::thread([this]() { server->wait(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(400)); // Wait for circuit breaker reset
        
        // Client should recover
        auto newSetResult = client.set(Key{"after_restart_" + std::to_string(restart)}, Value{"recovery"});
        EXPECT_TRUE(newSetResult.has_value());
    }
}

// Test client resilience with circuit breaker behavior during extended outage
TEST_F(KVStoreClientTest, CircuitBreakerDuringExtendedOutage) {
    // Use a policy with a low failure threshold to trigger circuit breaker quickly
    const RetryPolicy circuitBreakerPolicy{std::chrono::microseconds(10), std::chrono::microseconds(50), std::chrono::microseconds(200), 1, 1};
    Config c{addresses, circuitBreakerPolicy};
    KVStoreClient client{c};
    
    // Verify client works normally
    EXPECT_TRUE(client.set(Key{"before_outage"}, Value{"value"}).has_value());
    
    // Simulate extended server outage
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    // Multiple failed operations should trigger circuit breaker
    for (int i = 0; i < 5; ++i) {
        auto result = client.get(Key{"test"});
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
    }

    // Restart server
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    serverThread = std::thread([this]() { server->wait(); });
    std::this_thread::sleep_for(std::chrono::seconds(4)); // Wait for circuit breaker reset timeout
    
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
    KVStoreServiceImpl serviceImpl2{kvStore2}, serviceImpl3{kvStore3};
    std::unique_ptr<KVStoreServer> server2, server3;
    std::thread serverThread2, serverThread3;
    
    server2 = std::make_unique<KVStoreServer>(serverAddress2, serviceImpl2);
    server3 = std::make_unique<KVStoreServer>(serverAddress3, serviceImpl3);
    serverThread2 = std::thread([&server2]() { server2->wait(); });
    serverThread3 = std::thread([&server3]() { server3->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    const std::vector<std::string> multiAddresses{SERVER_ADDR, serverAddress2, serverAddress3};
    const RetryPolicy multiServicePolicy{std::chrono::microseconds(50), std::chrono::microseconds(200), std::chrono::microseconds(500), 2, 3};
    Config c{multiAddresses, multiServicePolicy};
    KVStoreClient client{c};
    
    // Verify client works with all services up
    EXPECT_TRUE(client.set(Key{"multi_test"}, Value{"value"}).has_value());
    
    // Shutdown first server
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
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
    if (serverThread3.joinable()) {
        serverThread3.join();
    }
    
    // Now all operations should fail
    auto failResult = client.get(Key{"failover1"});
    EXPECT_FALSE(failResult.has_value());
    EXPECT_EQ(failResult.error().code, ErrorCode::AllServicesUnavailable);
    
    // Restart one server
    server2 = std::make_unique<KVStoreServer>(serverAddress2, serviceImpl2);
    serverThread2 = std::thread([&server2]() { server2->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(600)); // Wait for circuit breaker reset
    
    // Client should recover with the restarted service
    auto recoveryResult = client.set(Key{"recovery"}, Value{"success"});
    EXPECT_TRUE(recoveryResult.has_value());
    
    // Cleanup remaining servers
    if (server2) {
        server2->shutdown();
    }
    if (serverThread2.joinable()) {
        serverThread2.join();
    }
}

// Test exponential backoff behavior during retries
TEST_F(KVStoreClientTest, ExponentialBackoffDuringRetries) {
    // Use a policy with noticeable delays for testing backoff
    const RetryPolicy backoffPolicy{std::chrono::microseconds(100), std::chrono::microseconds(500), std::chrono::microseconds(1000), 3, 1};
    Config c{addresses, backoffPolicy};
    KVStoreClient client{c};
    
    // Verify client works normally
    EXPECT_TRUE(client.set(Key{"backoff_test"}, Value{"value"}).has_value());
    
    // Shutdown server to trigger retries
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    // Measure time for failed operation (should include backoff delays)
    auto start = std::chrono::steady_clock::now();
    auto result = client.get(Key{"backoff_test"});
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
    // Should take at least the base delay time due to retries
    EXPECT_GE(duration.count(), 50); // At least some delay from retries
}

// Test client recovery with data persistence after server restart
TEST_F(KVStoreClientTest, DataPersistenceAfterServerRestart) {
    const RetryPolicy recoveryPolicy{std::chrono::microseconds(50), std::chrono::microseconds(200), std::chrono::microseconds(500), 2, 1};
    Config c{addresses, recoveryPolicy};
    KVStoreClient client{c};
    
    // Set initial data
    EXPECT_TRUE(client.set(Key{"persistent_key"}, Value{"persistent_value"}).has_value());
    auto initialSize = client.size();
    ASSERT_TRUE(initialSize.has_value());
    EXPECT_EQ(initialSize.value(), 1);
    
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    // Operations fail during outage
    EXPECT_FALSE(client.get(Key{"persistent_key"}).has_value());
    
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    serverThread = std::thread([this]() { server->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(600)); // Wait for circuit breaker reset
    
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
    const RetryPolicy rapidCyclePolicy{std::chrono::microseconds(20), std::chrono::microseconds(80), std::chrono::microseconds(300), 2, 1};
    Config c{addresses, rapidCyclePolicy};
    KVStoreClient client{c};
    
    // Perform rapid server restarts
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Quick operation
        auto setResult = client.set(Key{"cycle_" + std::to_string(cycle)}, Value{"value"});
        EXPECT_TRUE(setResult.has_value());
        
        // Quick shutdown and restart
        server->shutdown();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        
        // Very brief downtime
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        serverThread = std::thread([this]() { server->wait(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Final verification that client still works
    auto finalResult = client.set(Key{"final_test"}, Value{"success"});
    EXPECT_TRUE(finalResult.has_value());
}

// Test retry exhaustion with all services down
TEST_F(KVStoreClientTest, RetryExhaustionAllServicesDown) {
    const RetryPolicy exhaustionPolicy{std::chrono::microseconds(10), std::chrono::microseconds(50), std::chrono::microseconds(200), 2, 1};
    Config c{addresses, exhaustionPolicy};
    KVStoreClient client{c};
    
    // Verify client works initially
    EXPECT_TRUE(client.set(Key{"before_shutdown"}, Value{"value"}).has_value());
    
    // Shutdown server
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
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
    const RetryPolicy intermittentPolicy{std::chrono::microseconds(30), std::chrono::microseconds(120), std::chrono::milliseconds(100), 3, 1};
    Config c{addresses, intermittentPolicy};
    KVStoreClient client{c};
    
    // Pattern: work -> fail -> work -> fail -> work
    
    // 1. Initial work
    EXPECT_TRUE(client.set(Key{"intermittent1"}, Value{"value1"}).has_value());
    
    // 2. Simulate failure
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    EXPECT_FALSE(client.get(Key{"intermittent1"}).has_value());
    
    // 3. Restore service - wait much longer for circuit breaker reset
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    serverThread = std::thread([this]() { server->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Wait for circuit breaker reset
    EXPECT_TRUE(client.set(Key{"intermittent2"}, Value{"value2"}).has_value());
    
    // 4. Another failure
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    EXPECT_FALSE(client.get(Key{"intermittent2"}).has_value());
    
    // 5. Final restore - wait much longer for circuit breaker reset
    server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
    serverThread = std::thread([this]() { server->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Wait for circuit breaker reset
    EXPECT_TRUE(client.set(Key{"intermittent3"}, Value{"value3"}).has_value());
}
