#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "client/KVStoreClient.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Error.hpp"
#include "server/KVStoreServer.hpp"
#include "server/InMemoryKVStore.hpp"
#include "proto/kvStore.grpc.pb.h"
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
    RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
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
    KVStoreClient client {c};
    auto result = client.size();
    EXPECT_TRUE(result.has_value());
}

TEST_F(KVStoreClientTest, SetAndGetSuccess) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto set_result = client.set("foo", "bar");
    EXPECT_TRUE(set_result.has_value());
    auto get_result = client.get("foo");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "bar");
}

TEST_F(KVStoreClientTest, GetNonExistentKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto get_result = client.get("missing");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, OverwriteValue) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    client.set("foo", "bar");
    client.set("foo", "baz");
    auto get_result = client.get("foo");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "baz");
}

TEST_F(KVStoreClientTest, EraseExistingKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    client.set("foo", "bar");
    auto erase_result = client.erase("foo");
    ASSERT_TRUE(erase_result.has_value());
    EXPECT_EQ(erase_result.value(), "bar");
    auto get_result = client.get("foo");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, EraseNonExistentKey) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto erase_result = client.erase("missing");
    EXPECT_FALSE(erase_result.has_value());
    EXPECT_EQ(erase_result.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, SizeReflectsSetAndErase) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    client.set("a", "1");
    client.set("b", "2");
    auto size_result = client.size();
    ASSERT_TRUE(size_result.has_value());
    EXPECT_EQ(size_result.value(), 2);
    client.erase("a");
    size_result = client.size();
    ASSERT_TRUE(size_result.has_value());
    EXPECT_EQ(size_result.value(), 1);
}

TEST_F(KVStoreClientTest, FailureToConnectThrows) {
    std::vector<std::string> bad_addresses{"localhost:59999"};
    EXPECT_THROW({
        Config c(bad_addresses, policy);
        KVStoreClient client(c);
    }, std::runtime_error);
}

TEST_F(KVStoreClientTest, SetFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown(); // Simulate server down
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto set_result = client.set("foo", "bar");
    EXPECT_FALSE(set_result.has_value());
    EXPECT_EQ(set_result.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, GetFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto get_result = client.get("foo");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, EraseFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto erase_result = client.erase("foo");
    EXPECT_FALSE(erase_result.has_value());
    EXPECT_EQ(erase_result.error().code, ErrorCode::AllServicesUnavailable);
}

TEST_F(KVStoreClientTest, SizeFailureReturnsError) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    server->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto size_result = client.size();
    EXPECT_FALSE(size_result.has_value());
    EXPECT_EQ(size_result.error().code, ErrorCode::AllServicesUnavailable);
}

// Edge case: empty key
TEST_F(KVStoreClientTest, EmptyKeySetGetErase) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    auto set_result = client.set("", "empty");
    EXPECT_TRUE(set_result.has_value());
    auto get_result = client.get("");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "empty");
    auto erase_result = client.erase("");
    ASSERT_TRUE(erase_result.has_value());
    EXPECT_EQ(erase_result.value(), "empty");
}

// Edge case: large value
TEST_F(KVStoreClientTest, LargeValueSetGet) {
    Config c {addresses, policy};
    KVStoreClient client {c};
    std::string large_value(100000, 'x');
    auto set_result = client.set("big", large_value);
    EXPECT_TRUE(set_result.has_value());
    auto get_result = client.get("big");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), large_value);
}

// Test behavior with servicesToTry = 0 (should fail immediately)
TEST_F(KVStoreClientTest, ServicesToTryZeroFailsImmediately) {
    RetryPolicy zeroServicesPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 0};
    Config c{addresses, zeroServicesPolicy};
    KVStoreClient client{c};
    
    // Should fail immediately without trying any services
    auto result = client.get("test");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::AllServicesUnavailable);
}

// Test behavior with servicesToTry = 1 (should try only once)
TEST_F(KVStoreClientTest, ServicesToTryOneTriesOnlyOnce) {
    RetryPolicy oneServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 1};
    Config c{addresses, oneServicePolicy};
    KVStoreClient client{c};
    
    // Should work when service is available
    auto set_result = client.set("test", "value");
    EXPECT_TRUE(set_result.has_value());
    
    auto get_result = client.get("test");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "value");
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
    RetryPolicy oneServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 1};
    Config c1{multiAddresses, oneServicePolicy};
    KVStoreClient client1{c1};
    
    auto result1 = client1.set("key1", "value1");
    EXPECT_TRUE(result1.has_value());
    
    // Test with servicesToTry = 2 (should work with up to 2 services)
    RetryPolicy twoServicesPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 2};
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
    RetryPolicy multiServicePolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 3};
    Config c{addresses, multiServicePolicy};
    KVStoreClient client{c};
    
    // First verify it works
    auto set_result = client.set("test", "value");
    EXPECT_TRUE(set_result.has_value());
    
    // Now simulate service becoming unavailable by shutting down the server
    server->shutdown();
    if (serverThread.joinable()) {
        serverThread.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should fail after trying available services (up to servicesToTry limit)
    auto get_result = client.get("test");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::AllServicesUnavailable);
}

// Test edge case: servicesToTry larger than available services
TEST_F(KVStoreClientTest, ServicesToTryLargerThanAvailableServices) {
    // Policy tries 5 services but only 1 is available
    RetryPolicy excessiveRetryPolicy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2, 5};
    Config c{addresses, excessiveRetryPolicy};
    KVStoreClient client{c};
    
    // Should still work - client will try available services
    auto result = client.set("test", "value");
    EXPECT_TRUE(result.has_value());
    
    auto get_result = client.get("test");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "value");
}

// Test that Config properly exposes the RetryPolicy
TEST_F(KVStoreClientTest, ConfigExposesRetryPolicy) {
    RetryPolicy customPolicy{std::chrono::microseconds(200), std::chrono::microseconds(2000), std::chrono::microseconds(10000), 5, 3};
    Config c{addresses, customPolicy};
    
    // Verify the policy is properly stored and accessible
    EXPECT_EQ(c.policy.baseDelay, std::chrono::microseconds(200));
    EXPECT_EQ(c.policy.maxDelay, std::chrono::microseconds(2000));
    EXPECT_EQ(c.policy.resetTimeout, std::chrono::microseconds(10000));
    EXPECT_EQ(c.policy.failureThreshold, 5);
    EXPECT_EQ(c.policy.servicesToTry, 3);
}
