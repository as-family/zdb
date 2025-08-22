#ifndef RAFT_IMPL_H
#define RAFT_IMPL_H

#include "raft/Raft.hpp"
#include "raft/Types.hpp"
#include "raft/Channel.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <proto/raft.grpc.pb.h>
#include "common/RPCService.hpp"
#include "common/RetryPolicy.hpp"
#include <unordered_map>

namespace raft {

using Client = zdb::RPCService<proto::Raft>;

class RaftImpl : public Raft {
public:
    RaftImpl(std::vector<std::string> p, std::string s, Channel& c);
    AppendEntriesReply appendEntries(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVote(const RequestVoteArg& arg) override;
    ~RaftImpl();
private:
    Channel& channel;
    std::vector<std::string> peerAddresses;
    std::string selfId;
    zdb::RetryPolicy policy;
    std::unordered_map<std::string, Client> peers;
};

} // namespace raft

#endif // RAFT_IMPL_H
