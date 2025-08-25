#include <gtest/gtest.h>
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include <string>
#include <vector>
#include <thread>
#include <algorithm>
#include "common/Types.hpp"

raft::Command* createCommand(const std::string& cmd) {
    return nullptr;
}

int nRole(const std::vector<raft::Raft*>& rafts, raft::Role role) {
    int count = 0;
    for (const auto& raft : rafts) {
        if (raft->getRole() == role) {
            count++;
        }
    }
    return count;
}

bool check1Leader(const std::vector<raft::Raft*>& rafts) {
    return nRole(rafts, raft::Role::Leader) == 1 &&
           nRole(rafts, raft::Role::Follower) == rafts.size() - 1 &&
           nRole(rafts, raft::Role::Candidate) == 0;
}

TEST(Raft, InititialElection) {
    std::vector<raft::Channel> c{3};
    std::vector<std::string> v{"localhost:50051", "localhost:50052", "localhost:50053"};
    std::vector<raft::Raft*> r{};
    for (size_t i = 0; i < c.size(); ++i) {
        r.push_back(new raft::RaftImpl(v, v[i], c[i], createCommand));
    }
    EXPECT_EQ(r[0]->getSelfId(), "localhost:50051");
    EXPECT_EQ(r[1]->getSelfId(), "localhost:50052");
    EXPECT_EQ(r[2]->getSelfId(), "localhost:50053");

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_TRUE(check1Leader(r));

    for (auto raft : r) {
        delete raft;
    }
}

TEST(Raft, ReElection) {
    std::vector<raft::Channel> c{5};
    std::vector<std::string> v{"localhost:50051", "localhost:50052", "localhost:50053", "localhost:50054", "localhost:50055"};
    std::vector<raft::Raft*> r{};
    for (size_t i = 0; i < c.size(); ++i) {
        r.push_back(new raft::RaftImpl(v, v[i], c[i], createCommand));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_TRUE(check1Leader(r));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto term = r[0]->getCurrentTerm();
    EXPECT_TRUE(std::all_of(r.begin(), r.end(), [term](raft::Raft* raft) {
        return raft->getCurrentTerm() == term;
    }));

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_TRUE(std::all_of(r.begin(), r.end(), [term](raft::Raft* raft) {
        return raft->getCurrentTerm() == term;
    }));

    EXPECT_TRUE(check1Leader(r));

    auto leader = std::find_if(r.begin(), r.end(), [](raft::Raft* raft) {
        return raft->getRole() == raft::Role::Leader;
    });
    ASSERT_NE(leader, r.end());
    (*leader)->kill();
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_TRUE(check1Leader(r));
    for (auto raft : r) {
        delete raft;
    }
}
