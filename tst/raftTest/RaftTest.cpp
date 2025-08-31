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

zdb::RetryPolicy makePolicy(int servers) {
    return zdb::RetryPolicy {
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(50),
        std::chrono::milliseconds(60),
        10,
        10,
        std::chrono::milliseconds(4),
        std::chrono::milliseconds(4)
    };
}

TEST(Raft, InitialElection) {
    std::vector<EndPoints> config = makeConfig(3);
    zdb::RetryPolicy p = makePolicy(config.size());
    RAFTTestFramework framework{config, p};

    framework.check1Leader();
    EXPECT_EQ(framework.nRole(raft::Role::Leader), 1);
    EXPECT_EQ(framework.nRole(raft::Role::Candidate), 0);
    EXPECT_EQ(framework.nRole(raft::Role::Follower), 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto t1 = framework.checkTerms().value();
    EXPECT_GT(t1, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

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
    RAFTTestFramework framework{config, p};
    framework.check1Leader();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution dst{0, servers - 1};
    for (int ii = 0; ii < 10; ++ii) {
        auto i1 = dst(gen);
        auto i2 = dst(gen);
        auto i3 = dst(gen);
        config[i1].raftNetworkConfig.disconnect();
        config[i2].raftNetworkConfig.disconnect();
        config[i3].raftNetworkConfig.disconnect();
        framework.check1Leader();
        config[i1].raftNetworkConfig.connect();
        config[i2].raftNetworkConfig.connect();
        config[i3].raftNetworkConfig.connect();
    }
    framework.check1Leader();
}

TEST(Raft, BasicAgreeOneValue) {
    auto config = makeConfig(3);
    auto p = makePolicy(config.size());
    RAFTTestFramework framework{config, p};
    EXPECT_NO_THROW(framework.check1Leader());
    auto kvFrameworks = framework.getKVFrameworks();
    auto clientPolicy = zdb::RetryPolicy {
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(500),
        std::chrono::milliseconds(60),
        3,
        10,
        std::chrono::milliseconds(20),
        std::chrono::milliseconds(20)
    };
    auto r = kvFrameworks.begin()->second->spawnClientsAndWait(
        1,
        std::chrono::seconds(1),
        getKVProxies(config),
        clientPolicy,
        [](int id, zdb::KVStoreClient& client, std::atomic<bool>& done) {
            std::ignore = id;
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
    auto r2 = kvFrameworks.begin()->second->spawnClientsAndWait(
        1,
        std::chrono::seconds(1),
        getKVProxies(config),
        clientPolicy,
        [](int id, zdb::KVStoreClient& client, std::atomic<bool>& done) {
            std::ignore = id;
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
}
