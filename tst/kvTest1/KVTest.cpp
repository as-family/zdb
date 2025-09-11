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
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
#include "client/KVStoreClient.hpp"
#include "client/Config.hpp"
#include "common/RetryPolicy.hpp"
#include <chrono>
#include <thread>
#include "common/Types.hpp"
#include <raft/RaftImpl.hpp>
#include <raft/SyncChannel.hpp>
#include <raft/TestRaft.hpp>

TEST(KVTest, TestReliablePut) {
    NetworkConfig networkConfig {true, 0, 0};
    std::string targetAddress {"localhost:50052"};
    std::string proxyAddress {"localhost:50051"};
    raft::SyncChannel<std::unique_ptr<raft::Command>> leader{};
    TestRaft raft{leader};
    zdb::RetryPolicy proxyPolicy {
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{150L},
        std::chrono::milliseconds{200L},
        3,
        1,
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{20L}
    };
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig, leader, raft, proxyPolicy};
    zdb::Config config{{proxyAddress}, zdb::RetryPolicy{
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{150L},
        std::chrono::milliseconds{200L},
        10,
        1,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
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
    raft::SyncChannel<std::unique_ptr<raft::Command>> leader{};
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
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig, leader, raft, proxyPolicy};
    zdb::RetryPolicy policy{
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{500L},
        std::chrono::milliseconds{600L},
        100,
        2,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
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
    raft::SyncChannel<std::unique_ptr<raft::Command>> leader{};
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
    KVTestFramework kvTest {proxyAddress, targetAddress, networkConfig, leader, raft, proxyPolicy};
    zdb::RetryPolicy policy{
        std::chrono::microseconds{100L},
        std::chrono::microseconds{1000L},
        std::chrono::microseconds{5000L},
        10000,
        1,
        std::chrono::milliseconds{1000L},
        std::chrono::milliseconds{200L}
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
                    FAIL() << "shouldn't have happened more than once " << r.error().what;
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
