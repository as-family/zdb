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

TEST(Raft, InititialElection) {
    auto config = std::vector<std::tuple<std::string, std::string, NetworkConfig>>{
        {"localhost:50051", "localhost:50061", NetworkConfig{true, 0, 0}},
        {"localhost:50052", "localhost:50062", NetworkConfig{true, 0, 0}},
        {"localhost:50053", "localhost:50063", NetworkConfig{true, 0, 0}}
    };
    RAFTTestFramework framework{config};
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    for (auto& [id, raft] : framework.getRafts()) {
        std::cerr << "Raft " << id << " has term " << raft.getCurrentTerm() << std::endl;
    }
    EXPECT_EQ(framework.nRole(raft::Role::Leader), 1);
    EXPECT_EQ(framework.nRole(raft::Role::Candidate), 0);
    EXPECT_EQ(framework.nRole(raft::Role::Follower), 2);
}

TEST(Raft, ReElection) {
    auto config = std::vector<std::tuple<std::string, std::string, NetworkConfig>>{
        {"localhost:50051", "localhost:50061", NetworkConfig{true, 0, 0}},
        {"localhost:50052", "localhost:50062", NetworkConfig{true, 0, 0}},
        {"localhost:50053", "localhost:50063", NetworkConfig{true, 0, 0}},
        {"localhost:50054", "localhost:50064", NetworkConfig{true, 0, 0}},
        {"localhost:50055", "localhost:50065", NetworkConfig{true, 0, 0}},
        {"localhost:50056", "localhost:50066", NetworkConfig{true, 0, 0}},
        {"localhost:50057", "localhost:50067", NetworkConfig{true, 0, 0}}
    };
    RAFTTestFramework framework{config};
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    EXPECT_EQ(framework.nRole(raft::Role::Leader), 1);
    EXPECT_EQ(framework.nRole(raft::Role::Candidate), 0);
    EXPECT_EQ(framework.nRole(raft::Role::Follower), 6);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto term = framework.getRafts().begin()->second.getCurrentTerm();
    EXPECT_LT(term, 3);
    EXPECT_GT(term, 0);
    EXPECT_TRUE(std::all_of(framework.getRafts().begin(), framework.getRafts().end(), [term](const auto& pair) {
        return pair.second.getCurrentTerm() == term;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_TRUE(std::all_of(framework.getRafts().begin(), framework.getRafts().end(), [term](const auto& pair) {
        return pair.second.getCurrentTerm() == term;
    }));

    EXPECT_EQ(framework.nRole(raft::Role::Leader), 1);
    EXPECT_EQ(framework.nRole(raft::Role::Candidate), 0);
    EXPECT_EQ(framework.nRole(raft::Role::Follower), 6);
    auto leader = std::find_if(framework.getRafts().begin(), framework.getRafts().end(), [](const auto& pair) {
        return pair.second.getRole() == raft::Role::Leader;
    });
    ASSERT_NE(leader, framework.getRafts().end());
    leader->second.kill();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(framework.nRole(raft::Role::Leader), 1);
    EXPECT_EQ(framework.nRole(raft::Role::Candidate), 0);
    EXPECT_EQ(framework.nRole(raft::Role::Follower), 6);
}

TEST(Raft, ManyElections) {
    auto config = std::vector<std::tuple<std::string, std::string, NetworkConfig>>{
        {"localhost:50051", "localhost:50061", NetworkConfig{true, 0, 0}},
        {"localhost:50052", "localhost:50062", NetworkConfig{true, 0, 0}},
        {"localhost:50053", "localhost:50063", NetworkConfig{true, 0, 0}},
        {"localhost:50054", "localhost:50064", NetworkConfig{true, 0, 0}},
        {"localhost:50055", "localhost:50065", NetworkConfig{true, 0, 0}},
        {"localhost:50056", "localhost:50066", NetworkConfig{true, 0, 0}},
        {"localhost:50057", "localhost:50067", NetworkConfig{true, 0, 0}}
    };
    RAFTTestFramework framework{config};
    std::this_thread::sleep_for(std::chrono::seconds(1));

    framework.check1Leader();
}
