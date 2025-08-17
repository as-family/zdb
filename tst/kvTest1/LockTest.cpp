#include "lock/Lock.hpp"
#include "KVTestFramework.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/Types.hpp"
#include <string>
#include "NetworkConfig.hpp"
#include "common/RetryPolicy.hpp"
#include <gtest/gtest.h>

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
