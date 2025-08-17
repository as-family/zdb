#include "lock/Lock.hpp"
#include "KVTestFramework.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/Types.hpp"
#include <string>
#include "NetworkConfig.hpp"
#include "common/RetryPolicy.hpp"
#include <gtest/gtest.h>

const int nAcquire = 10;
const int nClient = 10;
const int nSec = 2;

KVTestFramework::ClientResult onClient(int clientId, zdb::KVStoreClient& client, std::atomic<bool>& done) {
    zdb::Key lockKey{"test_lock"};
    zdb::Lock lock(lockKey, client);
    client.set(zdb::Key{"testKey"}, zdb::Value{"testValue", 0});
    int i;
    for (i = 0; !done.load(); ++i) {
        lock.acquire();

        auto v = client.get(zdb::Key{"testKey"});
        if(!v.has_value())  {
            throw std::runtime_error("Failed to get value for testKey");
        }
        if(v.value().data != "testValue") {
            throw std::runtime_error("Unexpected value for testKey");
        }

        auto v2 = client.set(zdb::Key{"testKey"}, zdb::Value{std::to_string(clientId), v.value().version});
        if(!v2.has_value() && v2.error().code != zdb::ErrorCode::Maybe) {
            throw std::runtime_error("Failed to set value for testKey");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto v3 = client.set(zdb::Key{"testKey"}, zdb::Value{std::to_string(clientId), v.value().version + 1});
        if(!v3.has_value() && v3.error().code != zdb::ErrorCode::Maybe) {
            throw std::runtime_error("Failed to set value for testKey");
        }

        lock.release();
    }
    return KVTestFramework::ClientResult{i, 0}; // Assuming 1 OK and 0 Maybe for simplicity
}

void runClients(int nClients, bool reliable) {
    NetworkConfig networkConfig {reliable, 0.1, 0.1};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig};

    kvTest.spawnClientsAndWait(nClients, std::chrono::seconds(nSec), {proxyAddress}, zdb::RetryPolicy{
        std::chrono::microseconds(100),
        std::chrono::microseconds(1000),
        std::chrono::microseconds(5000),
        3,
        1
    }, onClient);
}

TEST(LockTest, TestOneClientReliable) {
    ASSERT_NO_THROW(runClients(1, true));
}

TEST(LockTest, AcquireLock) {
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
    auto client = kvTest.makeClient(c);
    zdb::Key lockKey{"test_lock"};
    zdb::Lock lock(lockKey, client);
    EXPECT_TRUE(lock.acquire());
}

TEST(LockTest, ReleaseLock) {
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
    auto client = kvTest.makeClient(c);
    zdb::Key lockKey{"test_lock"};
    zdb::Lock lock(lockKey, client);
    EXPECT_TRUE(lock.acquire());
    EXPECT_TRUE(lock.release());
}
