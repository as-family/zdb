// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
    std::shared_ptr<raft::SyncChannel<std::shared_ptr<raft::Command>>> leader = std::make_shared<raft::SyncChannel<std::shared_ptr<raft::Command>>>();
    std::shared_ptr<TestRaft> raft = std::make_shared<TestRaft>(*leader);
    zdb::RetryPolicy proxyPolicy {
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{150L},
        std::chrono::milliseconds{200L},
        1,
        1,
        std::chrono::milliseconds{10L},
        std::chrono::milliseconds{20L}
    };
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig, leader, raft, proxyPolicy};
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