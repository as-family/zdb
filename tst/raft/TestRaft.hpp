#ifndef RAFT_TEST_RAFT_H
#define RAFT_TEST_RAFT_H

#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/Types.hpp"
#include "raft/Log.hpp"

struct TestRaft : raft::Raft {
    TestRaft(raft::Channel& c) : channel {c}, mainLog{} {}
    raft::Start start(std::string cmd) override {
        channel.send(cmd);
        return {0, 0, true};
    }
    raft::AppendEntriesReply appendEntriesHandler(const raft::AppendEntriesArg& arg) override {
        std::ignore = arg;
        return {};
    }
    raft::RequestVoteReply requestVoteHandler(const raft::RequestVoteArg& arg) override {
        std::ignore = arg;
        return {};
    }
    void appendEntries(bool /*heartBeat*/) override {
    }
    void requestVote() override {
    }
    raft::Log& log() override {
        return mainLog;
    }
    void kill() override {
    }
    raft::Channel& channel;
private:
    raft::Log mainLog;
};

#endif // RAFT_TEST_RAFT_H
