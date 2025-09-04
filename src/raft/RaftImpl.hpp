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

template <typename Client>
class RaftImpl : public Raft {
public:
    RaftImpl(std::vector<std::string> p, std::string s, Channel& c, Channel& f, zdb::RetryPolicy r, std::function<Client&(std::string, zdb::RetryPolicy)> g);
    void appendEntries(bool heartBeat) override;
    void requestVote() override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    bool start(std::string command) override;
    void kill() override;
    Log& log() override;
    ~RaftImpl();
private:
    void applyCommittedEntries(Channel& channel);
    std::mutex m{};
    std::atomic<std::chrono::steady_clock::rep> time {};
    Channel& serviceChannel;
    Channel& followerChannel;
    zdb::RetryPolicy policy;
    zdb::FullJitter fullJitter;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat;
    Log mainLog;
    std::atomic<bool> killed;
    std::unordered_map<std::string, std::reference_wrapper<Client>> peers;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
};

template <typename Client>
RaftImpl<Client>::RaftImpl(std::vector<std::string> p, std::string s, Channel& c, Channel& f, zdb::RetryPolicy r, std::function<Client&(std::string, zdb::RetryPolicy)> g)
    : serviceChannel {c},
      followerChannel {f},
      policy {r},
      fullJitter {},
      lastHeartbeat {std::chrono::steady_clock::now()},
      mainLog {},
      electionTimer {},
      heartbeatTimer {} {
    selfId = s;
    clusterSize = p.size();
    nextIndex = std::unordered_map<std::string, uint64_t>{};
    matchIndex = std::unordered_map<std::string, uint64_t>{};
    for (const auto& a : p) {
        nextIndex[a] = 1;
        matchIndex[a] = 0;
    }
    p.erase(std::remove(p.begin(), p.end(), selfId), p.end());
    for (const auto& peer : p) {
        peers.emplace(peer, std::ref(g(peer, policy)));
    }
    electionTimeout = std::chrono::milliseconds(150);
    heartbeatInterval = std::chrono::milliseconds(30);

    electionTimer.start(
        [this] -> std::chrono::milliseconds {
            auto t = electionTimeout +
                   std::chrono::duration_cast<std::chrono::milliseconds>(fullJitter.jitter(
                    std::chrono::duration_cast<std::chrono::microseconds>(electionTimeout)));
            time = t.count();
            return t;
        },
        [this] {
            requestVote();
        }
    );
    heartbeatTimer.start(
        [this] -> std::chrono::milliseconds {
            return heartbeatInterval;
        },
        [this] {
            appendEntries(true);
        }
    );
}

template <typename Client>
RaftImpl<Client>::~RaftImpl() {
    std::unique_lock killLock {m};
    electionTimer.stop();
    heartbeatTimer.stop();
    for (auto& peer : peers) {
        peer.second.get().stop();
    }
    killed = true;
}

template <typename Client>
RequestVoteReply RaftImpl<Client>::requestVoteHandler(const RequestVoteArg& arg) {
    std::unique_lock lock{m};
    std::cerr << selfId << " received RequestVote from " << arg.candidateId << " for term " << arg.term << " (current term: " << currentTerm << ")\n";
    RequestVoteReply reply;
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.voteGranted = false;
        return reply;
    }
    if (arg.term > currentTerm) {
        currentTerm = arg.term;
        role = Role::Follower;
        lastHeartbeat = std::chrono::steady_clock::now();
        votedFor.reset();
    }
    if ((votedFor.has_value() && votedFor.value() == arg.candidateId) || !votedFor.has_value()) {
        if ((mainLog.lastTerm() == arg.lastLogTerm && mainLog.lastIndex() <= arg.lastLogIndex) || mainLog.lastTerm() < arg.lastLogTerm) {
            votedFor = arg.candidateId;
            lastHeartbeat = std::chrono::steady_clock::now();
            reply.term = currentTerm;
            reply.voteGranted = true;
            return reply;
        }
    }
    reply.term = currentTerm;
    reply.voteGranted = false;
    return reply;
}

template <typename Client>
AppendEntriesReply RaftImpl<Client>::appendEntriesHandler(const AppendEntriesArg& arg) {
    std::unique_lock lock{m};
    std::cerr << selfId << " received AppendEntries from " << arg.leaderId << " for term " << arg.term << " (current term: " << currentTerm << ")\n";
    AppendEntriesReply reply;
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
    if (arg.term > currentTerm) {
        currentTerm = arg.term;
        role = Role::Follower;
        votedFor.reset();
    }
    auto e = mainLog.at(arg.prevLogIndex);
    if (arg.prevLogIndex == 0 || (e.has_value() && e.value().term == arg.prevLogTerm)) {
        mainLog.merge(arg.entries);
        if (arg.leaderCommit > commitIndex) {
            commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
        }
        lastHeartbeat = std::chrono::steady_clock::now();
        applyCommittedEntries(followerChannel);
        reply.term = currentTerm;
        reply.success = true;
        return reply;
    } else {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
}

template <typename Client>
void RaftImpl<Client>::applyCommittedEntries(Channel& channel) {
    while (lastApplied < commitIndex) {
        ++lastApplied;
        auto c = mainLog.at(lastApplied);
        if (!channel.sendUntil(c.value().command, std::chrono::system_clock::now() + policy.rpcTimeout)) {
            --lastApplied;
            break;
        }
    }
}

template <typename Client>
void RaftImpl<Client>::appendEntries(bool heartBeat){
    std::unique_lock lock{m, std::defer_lock};
    if (heartBeat) {
        lock.lock();
    }
    if (role != Role::Leader) {
        return;
    }
    std::cerr << selfId << " sending AppendEntries for term " << currentTerm << "\n";
    std::vector<std::thread> threads;
    std::mutex threadsMutex{};
    int successCount = 1;
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
        [this, peerId, &threadsMutex, &successCount] {
                auto& peer = peers.at(peerId).get();
                auto n = nextIndex.at(peerId);
                auto v = mainLog.at(n);
                auto g = mainLog.suffix(v.has_value()? v.value().index : mainLog.lastIndex());
                proto::AppendEntriesArg arg;
                arg.set_term(currentTerm);
                arg.set_leaderid(selfId);
                auto prevLogIndex = n == 0? 0 : n - 1;
                if (prevLogIndex == 0) {
                    arg.set_prevlogindex(0);
                    arg.set_prevlogterm(0);
                } else {
                    arg.set_prevlogindex(prevLogIndex);
                    auto t = mainLog.at(prevLogIndex);
                    arg.set_prevlogterm(t.value().term);
                }
                arg.set_leadercommit(commitIndex);
                for (const auto& eg : g.data()) {
                    auto e = arg.add_entries();
                    e->set_index(eg.index);
                    e->set_term(eg.term);
                    e->set_command(eg.command);
                }
                proto::AppendEntriesReply reply;
                auto status = peer.call(
                    "appendEntries",
                    arg,
                    reply
                );
                std::unique_lock l{threadsMutex};
                if (role != Role::Leader) {
                    return;
                }
                if (status.has_value()) {
                    if (reply.success()) {
                        nextIndex[peerId] = g.lastIndex() + 1;
                        matchIndex[peerId] = g.lastIndex();
                        ++successCount;
                        if (successCount == clusterSize / 2 + 1) {
                            auto n = mainLog.lastIndex();
                            for (; n > commitIndex; --n) {
                                if (mainLog.at(n).has_value() && mainLog.at(n).value().term == currentTerm) {
                                    auto matches = 0;
                                    for (auto& [peerId, index] : matchIndex) {
                                        if (index >= n) {
                                            ++matches;
                                        }
                                    }
                                    if (matches + 1 > clusterSize / 2) {
                                        commitIndex = n;
                                        break;
                                    }
                                }
                            }
                            applyCommittedEntries(serviceChannel);
                        }
                    } else if (reply.term() > currentTerm) {
                        currentTerm = reply.term();
                        role = Role::Follower;
                    } else if (nextIndex[peerId] > 1) {
                        --nextIndex[peerId];
                    }
                }
            }
        );
    }
    std::this_thread::sleep_for(heartbeatInterval);
    for (auto& peer : peers) {
        peer.second.get().stop();
    }
    for (auto& t : threads) {
        if(t.joinable()) {
            t.join();
        }
    }
}

template <typename Client>
void RaftImpl<Client>::requestVote(){
    std::unique_lock lock{m};
    auto d = std::chrono::steady_clock::now() - lastHeartbeat;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(d) < std::chrono::milliseconds(time)) {
        return;
    }
    if (role == Role::Leader) {
        return;
    }
    std::cerr << selfId << " starting election for term " << currentTerm + 1 << "\n";
    role = Role::Candidate;
    ++currentTerm;
    votedFor = selfId;
    lastHeartbeat = std::chrono::steady_clock::now();
    int votesGranted = 1;
    std::vector<std::thread> threads;
    std::mutex threadsMutex{};
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
            [this, peerId, &votesGranted, &threadsMutex] {
                auto& peer = peers.at(peerId).get();
                proto::RequestVoteArg arg;
                arg.set_term(currentTerm);
                arg.set_candidateid(selfId);
                arg.set_lastlogindex(mainLog.lastIndex());
                arg.set_lastlogterm(mainLog.lastTerm());
                proto::RequestVoteReply reply;
                auto status = peer.call(
                    "requestVote",
                    arg,
                    reply
                );
                std::unique_lock l{threadsMutex};
                if (role != Role::Candidate) {
                    return;
                }
                if (status.has_value()) {
                    if (reply.votegranted()) {
                        ++votesGranted;
                        if (votesGranted == clusterSize / 2 + 1) {
                            role = Role::Leader;
                            std::cerr << selfId << " became leader for term " << currentTerm << "\n";
                            for (const auto& [a, _] : peers) {
                                nextIndex[a] = mainLog.lastIndex() + 1;
                                matchIndex[a] = 0;
                            }
                            appendEntries(false);
                        }
                    } else if (reply.term() > currentTerm) {
                        currentTerm = reply.term();
                        role = Role::Follower;
                        votedFor.reset();
                        lastHeartbeat = std::chrono::steady_clock::now();
                    }
                }
            }
        );
    }
    for (auto& t : threads) {
        if(t.joinable()) {
            t.join();
        }
    }
}

template <typename Client>
bool RaftImpl<Client>::start(std::string command) {
    std::unique_lock lock{m};
    if (role != Role::Leader) {
        return false;
    }
    LogEntry e {
        mainLog.lastIndex() + 1,
        currentTerm,
        command
    };
    mainLog.append(e);
    appendEntries(false);
    return true;
}

template <typename Client>
void RaftImpl<Client>::kill(){
    std::unique_lock killLock {m};
    killed = true;
}

template <typename Client>
Log& RaftImpl<Client>::log(){
    std::unique_lock lock{m};
    return mainLog;
}

} // namespace raft

#endif // RAFT_IMPL_H
