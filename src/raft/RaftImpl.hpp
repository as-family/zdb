#ifndef RAFT_IMPL_H
#define RAFT_IMPL_H

#include "raft/Raft.hpp"
#include "raft/Types.hpp"
#include "raft/Channel.hpp"
#include "raft/Log.hpp"
#include "raft/Command.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <proto/raft.grpc.pb.h>
#include "common/RPCService.hpp"
#include "common/RetryPolicy.hpp"
#include <unordered_map>
#include "raft/RaftServiceImpl.hpp"
#include "raft/AsyncTimer.hpp"
#include <atomic>
#include <functional>

namespace raft {

using Client = zdb::RPCService<proto::Raft>;

class RaftImpl : public Raft {
public:
    RaftImpl(std::vector<std::string> p, std::string s, Channel& c, Command* (*f)(const std::string&));
    void appendEntries() override;
    void requestVote() override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    void start(Command* command) override;
    void kill();
    Log* makeLog();
    Log& log() override;
    ~RaftImpl();
private:
    Channel& serviceChannel;
    zdb::RetryPolicy policy;
    RaftServiceImpl raftService;
    RaftServer server;
    zdb::FullJitter fullJitter;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat;
    std::vector<std::thread> threads;
    Command* (*commandFactory)(const std::string&);
    Log mainLog;
    std::atomic<bool> killed;
    std::unordered_map<std::string, Client> peers;

    unsigned int votesGranted;
    unsigned int downPeers;
    unsigned int votesDeclined;
    bool electionEnded;
    std::mutex electionMutex;
    std::condition_variable electionCondVar;
};

} // namespace raft

#endif // RAFT_IMPL_H
