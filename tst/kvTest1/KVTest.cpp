#include <gtest/gtest.h>
#include "NetworkConfig.hpp"
#include "KVTestFramework.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/RetryPolicy.hpp"
#include <chrono>
#include "common/Types.hpp"

TEST(KVTest, TestReliablePut) {
    NetworkConfig networkConfig {true, 0, 0};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig};
    zdb::Config config{{proxyAddress}, zdb::RetryPolicy{
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(5000),
        3,
        1
    }};
    zdb::KVStoreClient client = kvTest.makeClient(config);

    auto setResult = client.set(zdb::Key{"testKey"}, zdb::Value{"testValue", 0});
    ASSERT_TRUE(setResult.has_value());

    auto getResult = client.get(zdb::Key{"testKey"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_EQ(getResult->data, "testValue");
    ASSERT_EQ(getResult->version, 1);

    auto badVersionSetResult = client.set(zdb::Key{"testKey"}, zdb::Value{"newValue", 0});
    ASSERT_FALSE(badVersionSetResult.has_value());
    ASSERT_EQ(badVersionSetResult.error().code, zdb::ErrorCode::VersionMismatch);

    auto badInitVersionSetResult = client.set(zdb::Key{"k2"}, zdb::Value{"newValue", 1});
    ASSERT_FALSE(badInitVersionSetResult.has_value());
    ASSERT_EQ(badInitVersionSetResult.error().code, zdb::ErrorCode::KeyNotFound);

    auto badGetResult = client.get(zdb::Key{"k2"});
    ASSERT_FALSE(badGetResult.has_value());
    ASSERT_EQ(badGetResult.error().code, zdb::ErrorCode::KeyNotFound);
}

TEST(KVTest, TestPutConcurrentReliable) {
    NetworkConfig networkConfig {true, 0, 0};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig};
    zdb::RetryPolicy policy{
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(5000),
        3,
        1
    };
    const int nClients = 10;
    const std::chrono::seconds timeout(1);
    auto results = kvTest.spawnClientsAndWait(nClients, timeout, {proxyAddress}, policy, [&kvTest](int id, zdb::KVStoreClient& client, std::atomic<bool>& done) {
        return kvTest.oneClientSet(id, client, {zdb::Key{"k"}}, false, done);
    });
    zdb::Config config {{proxyAddress}, policy};
    auto client = kvTest.makeClient(config);
    ASSERT_TRUE(kvTest.checkSetConcurrent(client, zdb::Key{"k"}, results));
}
