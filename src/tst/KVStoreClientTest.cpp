#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "../KVStoreClient.hpp"
#include "../RetryPolicy.hpp"
#include "../Error.hpp"
#include "../KVStoreServer.hpp"
#include "../InMemoryKVStore.hpp"
#include <src/proto/kvStore.grpc.pb.h>
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
    RetryPolicy policy{std::chrono::microseconds(100), std::chrono::microseconds(1000), std::chrono::microseconds(5000), 2};
    std::vector<std::string> addresses{SERVER_ADDR};

    void SetUp() override {
        server = std::make_unique<KVStoreServer>(SERVER_ADDR, serviceImpl);
        serverThread = std::thread([this]() { server->wait(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    void TearDown() override {
        if (serverThread.joinable()) serverThread.detach();
    }
};

TEST_F(KVStoreClientTest, ConnectsToServer) {
    KVStoreClient client(addresses, policy);
    auto result = client.size();
    EXPECT_TRUE(result.has_value());
}

TEST_F(KVStoreClientTest, SetAndGetSuccess) {
    KVStoreClient client(addresses, policy);
    auto set_result = client.set("foo", "bar");
    EXPECT_TRUE(set_result.has_value());
    auto get_result = client.get("foo");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "bar");
}

TEST_F(KVStoreClientTest, GetNonExistentKey) {
    KVStoreClient client(addresses, policy);
    auto get_result = client.get("missing");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, OverwriteValue) {
    KVStoreClient client(addresses, policy);
    client.set("foo", "bar");
    client.set("foo", "baz");
    auto get_result = client.get("foo");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), "baz");
}

TEST_F(KVStoreClientTest, EraseExistingKey) {
    KVStoreClient client(addresses, policy);
    client.set("foo", "bar");
    auto erase_result = client.erase("foo");
    ASSERT_TRUE(erase_result.has_value());
    EXPECT_EQ(erase_result.value(), "bar");
    auto get_result = client.get("foo");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, EraseNonExistentKey) {
    KVStoreClient client(addresses, policy);
    auto erase_result = client.erase("missing");
    EXPECT_FALSE(erase_result.has_value());
    EXPECT_EQ(erase_result.error().code, ErrorCode::NotFound);
}

TEST_F(KVStoreClientTest, SizeReflectsSetAndErase) {
    KVStoreClient client(addresses, policy);
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
        KVStoreClient client(bad_addresses, policy);
    }, std::runtime_error);
}

TEST_F(KVStoreClientTest, SetFailureReturnsError) {
    KVStoreClient client(addresses, policy);
    server->~KVStoreServer(); // Simulate server down
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto set_result = client.set("foo", "bar");
    EXPECT_FALSE(set_result.has_value());
    EXPECT_EQ(set_result.error().code, ErrorCode::ServiceTemporarilyUnavailable);
}

TEST_F(KVStoreClientTest, GetFailureReturnsError) {
    KVStoreClient client(addresses, policy);
    server->~KVStoreServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto get_result = client.get("foo");
    EXPECT_FALSE(get_result.has_value());
    EXPECT_EQ(get_result.error().code, ErrorCode::ServiceTemporarilyUnavailable);
}

TEST_F(KVStoreClientTest, EraseFailureReturnsError) {
    KVStoreClient client(addresses, policy);
    server->~KVStoreServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto erase_result = client.erase("foo");
    EXPECT_FALSE(erase_result.has_value());
    EXPECT_EQ(erase_result.error().code, ErrorCode::ServiceTemporarilyUnavailable);
}

TEST_F(KVStoreClientTest, SizeFailureReturnsError) {
    KVStoreClient client(addresses, policy);
    server->~KVStoreServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto size_result = client.size();
    EXPECT_FALSE(size_result.has_value());
    EXPECT_EQ(size_result.error().code, ErrorCode::ServiceTemporarilyUnavailable);
}

// Edge case: empty key
TEST_F(KVStoreClientTest, EmptyKeySetGetErase) {
    KVStoreClient client(addresses, policy);
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
    KVStoreClient client(addresses, policy);
    std::string large_value(100000, 'x');
    auto set_result = client.set("big", large_value);
    EXPECT_TRUE(set_result.has_value());
    auto get_result = client.get("big");
    ASSERT_TRUE(get_result.has_value());
    EXPECT_EQ(get_result.value(), large_value);
}

