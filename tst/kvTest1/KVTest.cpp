#include <gtest/gtest.h>
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
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
    zdb::KVStoreClient client = zdb::KVStoreClient {config};

    auto setResult = client.set(zdb::Key{"testKey"}, zdb::Value{"testValue", 0});
    ASSERT_TRUE(setResult.has_value());

    auto getResult = client.get(zdb::Key{"testKey"});
    ASSERT_TRUE(getResult.has_value());
    ASSERT_EQ(getResult->data, "testValue");
    ASSERT_EQ(getResult->version, 1);

    auto badVersionSetResult = client.set(zdb::Key{"testKey"}, zdb::Value{"newValue", 0});
    ASSERT_FALSE(badVersionSetResult.has_value());
    ASSERT_TRUE(badVersionSetResult.error().code == zdb::ErrorCode::VersionMismatch ||
                badVersionSetResult.error().code == zdb::ErrorCode::Maybe);

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
    auto client = zdb::KVStoreClient {config};
    ASSERT_TRUE(kvTest.checkSetConcurrent(client, zdb::Key{"k"}, results));
    ASSERT_TRUE(kvTest.porcupine.check(10));
}

TEST(KVTest, TestUnreliableNet) {
    NetworkConfig networkConfig {false, 0.1, 0.1};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig};
    zdb::RetryPolicy policy{
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(5000),
        10000,
        1
    };
    zdb::Config c {{proxyAddress}, policy};
    auto client = zdb::KVStoreClient {c};
    const int nTries = 100;
    auto retried = false;
    for (uint64_t t = 0; t < nTries; ++t) {
        for (int i = 0; true; ++i) {
            auto r = kvTest.setJson(0, client, zdb::Key{"k"}, zdb::Value{std::to_string(i), t});
            if (r.has_value() || r.error().code != zdb::ErrorCode::Maybe) {
                if (i > 0 && (r.has_value() || r.error().code != zdb::ErrorCode::VersionMismatch)) {
                    FAIL() << "shouldn't have happen more than once " << r.error().what;
                }
                break;
            }
            retried = true;
        }
        auto value = kvTest.getJson(0, client, zdb::Key{"k"});
        if (value.version != t + 1) {
            FAIL() << "Wrong version " << value.version << " expect " << t + 1;
        }
        if (value.data != std::to_string(0)) {
            FAIL() << "Wrong value " << value.data << " expect " << std::to_string(0);
        }
    }
    if (!retried) {
        FAIL() << "never returned ErrMaybe";
    }
    ASSERT_TRUE(kvTest.porcupine.check(10));
}
