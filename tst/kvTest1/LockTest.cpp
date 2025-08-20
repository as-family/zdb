#include "lock/Lock.hpp"
#include "KVTestFramework.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/Types.hpp"
#include <string>
#include "NetworkConfig.hpp"
#include "common/RetryPolicy.hpp"
#include <gtest/gtest.h>

const int nClient = 10;
const int nSec = 2;

KVTestFramework::ClientResult oneClient(int clientId, zdb::KVStoreClient& client, std::atomic<bool>& done) {
    zdb::Key lockKey{"test_lock"};
    zdb::Lock lock(lockKey, client);
    auto t = client.set(zdb::Key{"testKey"}, zdb::Value{"testValue", 0});
    if (!t.has_value() && t.error().code != zdb::ErrorCode::Maybe && t.error().code != zdb::ErrorCode::VersionMismatch) {
        throw std::runtime_error("Failed to set initial value for testKey");
    }
    int i = 0;
    for (; !done.load(); ++i) {
        lock.acquire();

        auto v = client.get(zdb::Key{"testKey"});
        if(!v.has_value())  {
            throw std::runtime_error("Failed to get value for testKey");
        }
        if(v.value().data != "testValue") {
            throw std::runtime_error("Unexpected value for testKey i:" + std::to_string(i) + " v:" + v.value().data);
        }

        auto v2 = client.set(zdb::Key{"testKey"}, zdb::Value{std::to_string(clientId), v.value().version});
        if(!v2.has_value() && v2.error().code != zdb::ErrorCode::Maybe) {
            throw std::runtime_error("Failed to set value for testKey i:" + std::to_string(i) + " v2:" + (v2.has_value() ? "OK" : v2.error().what));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto v3 = client.set(zdb::Key{"testKey"}, zdb::Value{"testValue", v.value().version + 1});
        if(!v3.has_value() && v3.error().code != zdb::ErrorCode::Maybe) {
            throw std::runtime_error("Failed to set value for testKey i:" + std::to_string(i) + " v3:" + (v3.has_value() ? "OK" : v3.error().what));
        }

        lock.release();
    }
    return KVTestFramework::ClientResult{i, 0};
}

void runClients(int nClients, bool reliable) {
    NetworkConfig networkConfig {reliable, 0.1, 0.1};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig};

    kvTest.spawnClientsAndWait(nClients, std::chrono::seconds(nSec), {proxyAddress}, zdb::RetryPolicy{
        std::chrono::milliseconds(100),
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(5000),
        100,
        1
    }, oneClient);
}

TEST(LockTest, TestOneClientReliable) {
    ASSERT_NO_THROW(runClients(1, true));
}

TEST(LockTest, TestManyClientsReliable) {
    ASSERT_NO_THROW(runClients(nClient, true));
}

TEST(LockTest, TestOneClientUnreliable) {
    ASSERT_NO_THROW(runClients(1, false));
}

TEST(LockTest, TestManyClientsUnreliable) {
    ASSERT_NO_THROW(runClients(nClient, false));
}

TEST(LockTest, AcquireLock) {
    NetworkConfig networkConfig {true, 0.1, 0.1};
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
    zdb::Key lockKey{"test_lock"};
    zdb::Lock lock(lockKey, client);
    lock.acquire();
}

TEST(LockTest, ReleaseLock) {
    NetworkConfig networkConfig {true, 0.1, 0.1};
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
    zdb::Key lockKey{"test_lock"};
    zdb::Lock lock(lockKey, client);
    lock.acquire();
    lock.release();
}
