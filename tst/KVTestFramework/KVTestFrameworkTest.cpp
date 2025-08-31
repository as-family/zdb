#include <gtest/gtest.h>
#include "KVTestFramework.hpp"
#include "NetworkConfig.hpp"
#include "NetworkConfig.hpp"
#include <chrono>
#include <algorithm>
#include <thread>

TEST(KVTestFrameworkTest, SpawnClientsAndWaitCoordinatesResults) {
    NetworkConfig networkConfig {true, 0, 0};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig};
    zdb::RetryPolicy policy {
        std::chrono::milliseconds(100),
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(5000),
        3,
        1,
        std::chrono::milliseconds(1000),
        std::chrono::milliseconds(200)
    };
    auto r = kvTest.spawnClientsAndWait(10, std::chrono::seconds(5), {proxyAddress}, policy,
        [](int id, zdb::KVStoreClient& client, std::atomic<bool>& done) {
            while(!done.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            return KVTestFramework::ClientResult{id, id % 2};
        });
    EXPECT_EQ(r.size(), 10);
    for (int i = 0; i < 10; ++i) {
        EXPECT_NE(std::find_if(r.begin(), r.end(), [i] (const KVTestFramework::ClientResult& a) { return a.nOK == i; }), r.end());
    }
}