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
    RaftImpl(std::vector<std::string> p, std::string s, Channel& c, zdb::RetryPolicy r, Command* (*f)(const std::string&));
    void appendEntries() override;
    void requestVote() override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    bool start(Command* command) override;
    void kill() override;
    Log* makeLog() override;
    Log& log() override;
    void connectPeers();
    ~RaftImpl();
private:
    std::mutex m{};
    std::condition_variable cv{};
    std::mutex commitIndexMutex{};
    std::mutex appendEntriesMutex{};
    std::atomic<uint64_t> time {};
    Channel& serviceChannel;
    zdb::RetryPolicy policy;
    zdb::FullJitter fullJitter;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat;
    Command* (*commandFactory)(const std::string&);
    Log mainLog;
    std::atomic<bool> killed;
    std::unordered_map<std::string, Client> peers;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
};

} // namespace raft

#endif // RAFT_IMPL_H
