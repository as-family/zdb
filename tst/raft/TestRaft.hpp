#ifndef RAFT_TEST_RAFT_H
#define RAFT_TEST_RAFT_H

#include "raft/Raft.hpp"
#include "raft/Command.hpp"
#include "raft/Channel.hpp"
#include "raft/Types.hpp"
#include <tuple>

struct TestRaft : raft::Raft {
    TestRaft(raft::Channel& c) : channel {c} {}
    bool start(std::string cmd) override {
        channel.send(cmd);
        return true;
    }
    raft::AppendEntriesReply appendEntriesHandler(const raft::AppendEntriesArg& arg) override {
        std::ignore = arg;
        return {};
    }
    raft::RequestVoteReply requestVoteHandler(const raft::RequestVoteArg& arg) override {
        std::ignore = arg;
        return {};
    }
    void appendEntries() override {
    }
    void requestVote() override {
    }
    raft::Log& log() override {
        static raft::Log dummyLog;
        return dummyLog;
    }
    void kill() override {
    }
    raft::Channel& channel;
};

#endif // RAFT_TEST_RAFT_H
