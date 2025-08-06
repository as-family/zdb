#include <gtest/gtest.h>
#include "client/KVStoreClient.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include <thread>
#include <chrono>
#include <string>

using namespace zdb;

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
    auto setResult = client.set("foo", "bar");
    EXPECT_TRUE(setResult.has_value());
    auto getResult = client.get("foo");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), "bar");
}

TEST_F(KVStoreClientTest, GetNonExistentKey) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    auto getResult = client.get("missing");
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, OverwriteValue) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    client.set("foo", "bar");
    client.set("foo", "baz");
    auto getResult = client.get("foo");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), "baz");
}

TEST_F(KVStoreClientTest, EraseExistingKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    client.set("foo", "bar");
    auto eraseResult = client.erase("foo");
    ASSERT_TRUE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.value(), "bar");
    auto getResult = client.get("foo");
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, EraseNonExistentKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto eraseResult = client.erase("missing");
    EXPECT_FALSE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, SizeReflectsSetAndErase) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    client.set("a", "1");
    client.set("b", "2");
    auto sizeResult = client.size();
    ASSERT_TRUE(sizeResult.has_value());
    EXPECT_EQ(sizeResult.value(), 2);
    client.erase("a");
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
    auto setResult = client.set("foo", "bar");
    EXPECT_FALSE(setResult.has_value());
    EXPECT_EQ(setResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, GetFailureReturnsError) {
    Config c {addresses, policy};
    const KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto getResult = client.get("foo");
    EXPECT_FALSE(getResult.has_value());
    EXPECT_EQ(getResult.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, EraseFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto eraseResult = client.erase("foo");
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
    auto setResult = client.set("", "empty");
    EXPECT_TRUE(setResult.has_value());
    auto getResult = client.get("");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), "empty");
    auto eraseResult = client.erase("");
    ASSERT_TRUE(eraseResult.has_value());
    EXPECT_EQ(eraseResult.value(), "empty");
}

// Edge case: large value
TEST_F(KVStoreClientTest, LargeValueSetGet) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    const std::string largeValue(100000, 'x');
    auto setResult = client.set("big", largeValue);
    EXPECT_TRUE(setResult.has_value());
    auto getResult = client.get("big");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), largeValue);
}

// Test behavior with servicesToTry = 0 (should fail immediately)
TEST_F(KVStoreClientTest, ServicesToTryZeroFailsImmediately) {
    const RetryPolicy zeroServicesPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 0};
    Config c{addresses, zeroServicesPolicy};
    const KVStoreClient client{c};
    
    // Should fail immediately without trying any services
    auto result = client.get("test");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
}

// Test behavior with servicesToTry = 1 (should try only once)
TEST_F(KVStoreClientTest, ServicesToTryOneTriesOnlyOnce) {
    const RetryPolicy oneServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 1};
    Config c{addresses, oneServicePolicy};
    KVStoreClient client{c};
    
    // Should work when service is available
    auto setResult = client.set("test", "value");
    EXPECT_TRUE(setResult.has_value());
    
    auto getResult = client.get("test");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), "value");
}

// Test with multiple services and different servicesToTry values
TEST_F(KVStoreClientTest, MultipleServicesWithVariousRetryLimits) {
    // Set up additional server
    const std::string SERVER_ADDR2 = "localhost:50053";
    InMemoryKVStore kvStore2;
    KVStoreServiceImpl serviceImpl2{kvStore2};
    std::unique_ptr<KVStoreServer> server2;
    std::thread serverThread2;
    
    server2 = std::make_unique<KVStoreServer>(SERVER_ADDR2, serviceImpl2);
    serverThread2 = std::thread([&server2]() { server2->wait(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::vector<std::string> multiAddresses{SERVER_ADDR, SERVER_ADDR2};
    
    // Test with servicesToTry = 1 (should work with first available service)
    const RetryPolicy oneServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 1};
    Config c1{multiAddresses, oneServicePolicy};
    KVStoreClient client1{c1};
    
    auto result1 = client1.set("key1", "value1");
    EXPECT_TRUE(result1.has_value());
    
    // Test with servicesToTry = 2 (should work with up to 2 services)
    const RetryPolicy twoServicesPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
    Config c2{multiAddresses, twoServicesPolicy};
    KVStoreClient client2{c2};
    
    auto result2 = client2.set("key2", "value2");
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
    auto setResult = client.set("test", "value");
    EXPECT_TRUE(setResult.has_value());
    
    // Now simulate service becoming unavailable by shutting down the server
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should fail after trying available services (up to servicesToTry limit)
    auto getResult = client.get("test");
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
    auto result = client.set("test", "value");
    EXPECT_TRUE(result.has_value());
    
    auto getResult = client.get("test");
    ASSERT_TRUE(getResult.has_value());
    EXPECT_EQ(getResult.value(), "value");
}

// Test that Config properly exposes the RetryPolicy
TEST_F(KVStoreClientTest, ConfigExposesRetryPolicy) {
    const RetryPolicy customPolicy{std::chrono::microseconds(200), std::chrono::microseconds(2000), std::chrono::microseconds(10000), 5, 3};
    Config c{addresses, customPolicy};
    
    // Verify the policy is properly stored and accessible
    EXPECT_EQ(c.policy.baseDelay, std::chrono::microseconds(200));
    EXPECT_EQ(c.policy.maxDelay, std::chrono::microseconds(2000));
    EXPECT_EQ(c.policy.resetTimeout, std::chrono::microseconds(10000));
    EXPECT_EQ(c.policy.failureThreshold, 5);
    EXPECT_EQ(c.policy.servicesToTry, 3);
}
