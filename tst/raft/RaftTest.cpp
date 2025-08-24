#include <gtest/gtest.h>
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

TEST(Raft, Init) {
    std::vector<raft::Channel> c{3};
    std::vector<std::string> v{"localhost:50051", "localhost:50052", "localhost:50053"};
    std::vector<raft::Raft*> r{};
    for (size_t i = 0; i < c.size(); ++i) {
        r.push_back(new raft::RaftImpl(v, v[i], c[i]));
    }
    EXPECT_EQ(r[0]->selfId, "localhost:50051");
    EXPECT_EQ(r[1]->selfId, "localhost:50052");
    EXPECT_EQ(r[2]->selfId, "localhost:50053");
    EXPECT_EQ(r[0]->peerAddresses.size(), 2);
    EXPECT_EQ(r[1]->peerAddresses.size(), 2);
    EXPECT_EQ(r[2]->peerAddresses.size(), 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_EQ((r[0]->role == raft::Role::Leader) +
              (r[1]->role == raft::Role::Leader) +
              (r[2]->role == raft::Role::Leader), 1);
    EXPECT_EQ((r[0]->role == raft::Role::Follower) +
              (r[1]->role == raft::Role::Follower) +
              (r[2]->role == raft::Role::Follower), 2);
    EXPECT_EQ((r[0]->role == raft::Role::Candidate) +
              (r[1]->role == raft::Role::Candidate) +
              (r[2]->role == raft::Role::Candidate), 0);
    for (auto raft : r) {
        delete raft;
    }
}

TEST(Raft, Init5) {
    std::vector<raft::Channel> c{5};
    std::vector<std::string> v{"localhost:50051", "localhost:50052", "localhost:50053", "localhost:50054", "localhost:50055"};
    std::vector<raft::Raft*> r{};
    for (size_t i = 0; i < c.size(); ++i) {
        r.push_back(new raft::RaftImpl(v, v[i], c[i]));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_EQ((r[0]->role == raft::Role::Leader) +
              (r[1]->role == raft::Role::Leader) +
              (r[2]->role == raft::Role::Leader) +
              (r[3]->role == raft::Role::Leader) +
              (r[4]->role == raft::Role::Leader), 1);
    EXPECT_EQ((r[0]->role == raft::Role::Follower) +
              (r[1]->role == raft::Role::Follower) +
              (r[2]->role == raft::Role::Follower) +
              (r[3]->role == raft::Role::Follower) +
              (r[4]->role == raft::Role::Follower), 4);
    EXPECT_EQ((r[0]->role == raft::Role::Candidate) +
              (r[1]->role == raft::Role::Candidate) +
              (r[2]->role == raft::Role::Candidate) +
              (r[3]->role == raft::Role::Candidate) +
              (r[4]->role == raft::Role::Candidate), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto term = r[0]->currentTerm;
    EXPECT_TRUE(std::all_of(r.begin(), r.end(), [term](raft::Raft* raft) {
        return raft->currentTerm == term;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_TRUE(std::all_of(r.begin(), r.end(), [term](raft::Raft* raft) {
        return raft->currentTerm == term;
    }));

    EXPECT_EQ((r[0]->role == raft::Role::Leader) +
              (r[1]->role == raft::Role::Leader) +
              (r[2]->role == raft::Role::Leader) +
              (r[3]->role == raft::Role::Leader) +
              (r[4]->role == raft::Role::Leader), 1);
    EXPECT_EQ((r[0]->role == raft::Role::Follower) +
              (r[1]->role == raft::Role::Follower) +
              (r[2]->role == raft::Role::Follower) +
              (r[3]->role == raft::Role::Follower) +
              (r[4]->role == raft::Role::Follower), 4);
    EXPECT_EQ((r[0]->role == raft::Role::Candidate) +
              (r[1]->role == raft::Role::Candidate) +
              (r[2]->role == raft::Role::Candidate) +
              (r[3]->role == raft::Role::Candidate) +
              (r[4]->role == raft::Role::Candidate), 0);

    auto leader = std::find_if(r.begin(), r.end(), [](raft::Raft* raft) {
        return raft->role == raft::Role::Leader;
    });
    ASSERT_NE(leader, r.end());
    (*leader)->kill();
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_EQ((r[0]->role == raft::Role::Leader) +
              (r[1]->role == raft::Role::Leader) +
              (r[2]->role == raft::Role::Leader) +
              (r[3]->role == raft::Role::Leader) +
              (r[4]->role == raft::Role::Leader), 1);
    EXPECT_EQ((r[0]->role == raft::Role::Follower) +
              (r[1]->role == raft::Role::Follower) +
              (r[2]->role == raft::Role::Follower) +
              (r[3]->role == raft::Role::Follower) +
              (r[4]->role == raft::Role::Follower), 4);
    for (auto raft : r) {
        delete raft;
    }
}
