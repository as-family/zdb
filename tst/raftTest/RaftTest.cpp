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

TEST(Raft, InititialElection) {
    std::vector<EndPoints> config {
        {"localhost:50051", "localhost:50061", "localhost:50071", "localhost:50081", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50052", "localhost:50062", "localhost:50072", "localhost:50082", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50053", "localhost:50063", "localhost:50073", "localhost:50083", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}}
    };
    zdb::RetryPolicy p {
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(50),
        std::chrono::minutes(1),
        10,
        1
    };
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
    std::vector<EndPoints> config {
        {"localhost:50051", "localhost:50061", "localhost:50071", "localhost:50081", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50052", "localhost:50062", "localhost:50072", "localhost:50082", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50053", "localhost:50063", "localhost:50073", "localhost:50083", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
    };
    std::vector<std::string> raftTargets {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"
    };
    zdb::RetryPolicy p {
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(50),
        std::chrono::minutes(1),
        10,
        1
    };
    RAFTTestFramework framework{config, p};

    auto leader1 = framework.check1Leader();
    framework.disconnect(leader1);
    std::cerr << "------------------------------------------- disconnect" << std::endl;
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
    std::vector<EndPoints> config {
        {"localhost:50051", "localhost:50061", "localhost:50071", "localhost:50081", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50052", "localhost:50062", "localhost:50072", "localhost:50082", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50053", "localhost:50063", "localhost:50073", "localhost:50083", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50054", "localhost:50064", "localhost:50074", "localhost:50084", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50055", "localhost:50065", "localhost:50075", "localhost:50085", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50056", "localhost:50066", "localhost:50076", "localhost:50086", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}},
        {"localhost:50057", "localhost:50067", "localhost:50077", "localhost:50087", NetworkConfig{true, 0, 0}, NetworkConfig{true, 0, 0}}
    };
    zdb::RetryPolicy p {
        std::chrono::milliseconds(10),
        std::chrono::milliseconds(50),
        std::chrono::minutes(1),
        3,
        1
    };
    RAFTTestFramework framework{config, p};
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    std::random_device rd;
    
    std::mt19937 gen(rd());
    std::uniform_int_distribution dst{0, 9};
    for (int ii = 0; ii < 10; ++ii) {
        auto i1 = dst(gen);
        auto i2 = dst(gen);
        auto i3 = dst(gen);
        config[i1].raftNetworkConfig.disconnect();
        config[i2].raftNetworkConfig.disconnect();
        config[i3].raftNetworkConfig.disconnect();

    }
}

