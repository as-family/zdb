#include <gtest/gtest.h>
#include "KVTestFramework.hpp"
#include "NetworkConfig.hpp"
#include "NetworkConfig.hpp"
#include <chrono>
#include <algorithm>
#include <thread>
#include "raft/SyncChannel.hpp"
#include "raft/TestRaft.hpp"

TEST(KVTestFrameworkTest, SpawnClientsAndWaitCoordinatesResults) {
    NetworkConfig networkConfig {true, 0, 0};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    raft::SyncChannel leader{};
    raft::SyncChannel follower{};
    TestRaft raft{leader};
    zdb::RetryPolicy proxyPolicy {
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{150L},
        std::chrono::milliseconds{200L},
        1,
        1,
        std::chrono::milliseconds{10L},
        std::chrono::milliseconds{20L}
    };
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig, leader, follower, raft, proxyPolicy};
    zdb::RetryPolicy policy {
        std::chrono::milliseconds{100L},
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{5000L},
        3,
        1,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
    };
    auto r = kvTest.spawnClientsAndWait(10, std::chrono::seconds{5}, {proxyAddress}, policy,
        [](int id, zdb::KVStoreClient& /*client*/, std::atomic<bool>& done) {
            while(!done.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds{100L});
            }
            return KVTestFramework::ClientResult{id, id};
        });
    EXPECT_EQ(r.size(), 10);
    for (int i = 0; i < 10; ++i) {
        EXPECT_NE(std::find_if(r.begin(), r.end(), [i] (const KVTestFramework::ClientResult& a) { return a.nOK == i; }), r.end());
    }
}