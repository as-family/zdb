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
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include "common/Types.hpp"
#include "RaftTestFramework/RaftTestFramework.hpp"
#include <random>

std::vector<EndPoints> makeConfig(int n) {
    std::vector<EndPoints> config;
    for (int i = 0; i < n; ++i) {
        config.push_back(
            EndPoints {
                "localhost:" + std::to_string(50051 + i),
                "localhost:" + std::to_string(50061 + i),
                "localhost:" + std::to_string(50071 + i),
                "localhost:" + std::to_string(50081 + i),
                NetworkConfig{true, 0, 0},
                NetworkConfig{true, 0, 0}
            }
        );
    }
    return config;
}

std::vector<std::string> getProxies(std::vector<EndPoints>& config) {
    std::vector<std::string> proxies(config.size());
    std::transform(
        config.begin(),
        config.end(),
        proxies.begin(),
        [](const auto& e) { return e.raftProxy; }
    );
    return proxies;
}

std::vector<std::string> getKVProxies(std::vector<EndPoints>& config) {
    std::vector<std::string> proxies(config.size());
    std::transform(
        config.begin(),
        config.end(),
        proxies.begin(),
        [](const auto& e) { return e.kvProxy; }
    );
    return proxies;
}

std::vector<std::string> getRaftTargets(std::vector<EndPoints>& config) {
    std::vector<std::string> targets(config.size());
    std::transform(
        config.begin(),
        config.end(),
        targets.begin(),
        [](const auto& e) { return e.raftTarget; }
    );
    return targets;
}

zdb::RetryPolicy makePolicy(int /*servers*/) {
    return zdb::RetryPolicy {
        std::chrono::milliseconds{10L},
        std::chrono::milliseconds{50L},
        std::chrono::milliseconds{60L},
        10,
        10,
        std::chrono::milliseconds{10L},
        std::chrono::milliseconds{10L}
    };
}

TEST(Raft, InitialElection) {
    std::vector<EndPoints> config = makeConfig(3);
    zdb::RetryPolicy p = makePolicy(config.size());
    RAFTTestFramework framework{config, p};

    framework.check1Leader();

    std::this_thread::sleep_for(std::chrono::milliseconds{50L});
    auto t1 = framework.checkTerms().value();
    EXPECT_GT(t1, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds{600L});
    auto t2 = framework.checkTerms().value();
    EXPECT_EQ(t1, t2);
    framework.check1Leader();
}

TEST(Raft, ReElection) {
    auto config = makeConfig(3);
    std::vector<std::string> raftTargets {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"
    };
    auto p = makePolicy(config.size());
    RAFTTestFramework framework{config, p};

    auto leader1 = framework.check1Leader();
    framework.disconnect(leader1);
    try {
        framework.check1Leader();
    } catch (const std::runtime_error& e) {
        std::ostringstream oss;
        for (const auto& term : framework.terms()) {
            oss << term << " ";
        }
        FAIL() << "No new leader elected after leader disconnection in term " << oss.str();
    }
    framework.connect(leader1);
    auto leader2 = framework.check1Leader();
    EXPECT_NE(leader1, leader2);

    framework.disconnect(leader2);
    auto i = std::find(raftTargets.begin(), raftTargets.end(), leader2);
    framework.disconnect(*(i == raftTargets.begin() ? i + 1 : raftTargets.begin()));
    std::this_thread::sleep_for(std::chrono::milliseconds{300L});

    framework.checkNoLeader();

    framework.connect(*(i == raftTargets.begin() ? i + 1 : raftTargets.begin()));
    auto leader3 = framework.check1Leader();

    framework.connect(leader2);
    auto leader4 = framework.check1Leader();
    EXPECT_EQ(leader3, leader4);
}

TEST(Raft, ManyElections) {
    auto servers = 7;
    auto config = makeConfig(7);
    auto p = makePolicy(config.size());
    auto targets = getRaftTargets(config);
    RAFTTestFramework framework{config, p};
    framework.check1Leader();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution dst{0, servers - 1};
    for (int ii = 0; ii < 10; ++ii) {
        auto i1 = dst(gen);
        auto i2 = dst(gen);
        auto i3 = dst(gen);
        framework.disconnect(targets[i1]);
        framework.disconnect(targets[i2]);
        framework.disconnect(targets[i3]);
        framework.check1Leader();
        framework.connect(targets[i1]);
        framework.connect(targets[i2]);
        framework.connect(targets[i3]);
    }
    framework.check1Leader();
}

TEST(Raft, BasicAgreeOneValue) {
    auto config = makeConfig(3);
    auto p = makePolicy(config.size());
    RAFTTestFramework framework{config, p};
    EXPECT_NO_THROW(framework.check1Leader());
    auto& kvFrameworks = framework.getKVFrameworks(config[0].raftTarget);
    auto clientPolicy = zdb::RetryPolicy {
        std::chrono::milliseconds{10L},
        std::chrono::milliseconds{500L},
        std::chrono::milliseconds{60L},
        3,
        10,
        std::chrono::milliseconds{20L},
        std::chrono::milliseconds{20L}
    };
    auto r = kvFrameworks.spawnClientsAndWait(
        1,
        std::chrono::seconds{1},
        getKVProxies(config),
        clientPolicy,
        [](int id, zdb::KVStoreClient& client, std::atomic<bool>& done) {
            std::ignore = id;
            std::ignore = done;
            int nOK = 0;
            int nMaybe = 0;
            auto res = client.set(zdb::Key {"key"}, zdb::Value{"value", 0});
            if (res.has_value()) {
                nOK++;
            } else {
                if (res.error().code == zdb::ErrorCode::Maybe) {
                    nMaybe++;
                }
            }
            return KVTestFramework::ClientResult{nOK, nMaybe};
        }
    );
    EXPECT_EQ(r[0].nOK, 1);
    auto r2 = kvFrameworks.spawnClientsAndWait(
        1,
        std::chrono::seconds{1},
        getKVProxies(config),
        clientPolicy,
        [](int id, zdb::KVStoreClient& client, std::atomic<bool>& done) {
            std::ignore = id;
            std::ignore = done;
            int nOK = 0;
            int nMaybe = 0;
            auto res = client.get(zdb::Key {"key"});
            if (res.has_value()) {
                if (res.value().data == "value" && res.value().version == 1) {
                    nOK++;
                }
            }
            return KVTestFramework::ClientResult{nOK, nMaybe};
        }
    );
    EXPECT_EQ(r2[0].nOK, 1);
}

TEST(Raft, BasicAgree) {
    auto config = makeConfig(3);
    auto p = makePolicy(config.size());
    RAFTTestFramework framework{config, p};
    EXPECT_NO_THROW(framework.check1Leader());
    for (int i = 1; i <= 3; ++i) {
        auto uuid = generate_uuid_v7();
        auto c = zdb::Get{uuid, zdb::Key{"key"}};
        auto nd = framework.nCommitted(i).first;
        ASSERT_EQ(nd, 0);
        auto xi = framework.one(c.serialize(), 3, false);
        ASSERT_EQ(xi, i);
    }
}
