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
    RaftImpl(std::vector<std::string> p, std::string s, Channel& c, zdb::RetryPolicy r, Command* (*f)(const std::string&), std::function<Client*(std::string, zdb::RetryPolicy)> g);
    void appendEntries() override;
    void requestVote() override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    bool start(Command* command) override;
    void kill() override;
    Log* makeLog() override;
    Log& log() override;
    ~RaftImpl();
private:
    std::mutex m{};
    std::condition_variable cv{};
    std::mutex commitIndexMutex{};
    std::mutex appendEntriesMutex{};
    std::atomic<std::chrono::steady_clock::rep> time {};
    Channel& serviceChannel;
    zdb::RetryPolicy policy;
    zdb::FullJitter fullJitter;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat;
    Command* (*commandFactory)(const std::string&);
    Log mainLog;
    std::atomic<bool> killed;
    std::unordered_map<std::string, Client*> peers;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
};

template <typename Client>
RaftImpl<Client>::RaftImpl(std::vector<std::string> p, std::string s, Channel& c, zdb::RetryPolicy r, Command* (*f)(const std::string&), std::function<Client*(std::string, zdb::RetryPolicy)> g)
    : serviceChannel {c},
      policy {r},
      fullJitter {},
      lastHeartbeat {std::chrono::steady_clock::now()},
      commandFactory {f},
      mainLog {f},
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
        peers[peer] = g(peer, policy);
    }
    electionTimeout = std::chrono::milliseconds(150);
    heartbeatInterval = std::chrono::milliseconds(20);

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
            appendEntries();
        }
    );
}

template <typename Client>
RequestVoteReply RaftImpl<Client>::requestVoteHandler(const RequestVoteArg& arg) {
    RequestVoteReply reply;
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.voteGranted = false;
        return reply;
    }
    if (arg.term > currentTerm) {
        std::unique_lock termLock {m};
        currentTerm = arg.term;
        role = Role::Follower;
        lastHeartbeat = std::chrono::steady_clock::now();
        votedFor = arg.candidateId;
        termLock.unlock();
        reply.voteGranted = true;
        return reply;
    }
    if (votedFor.has_value() && votedFor.value() == arg.candidateId || !votedFor.has_value()) {
        if (mainLog.lastTerm() == arg.lastLogTerm && mainLog.lastIndex() <= arg.lastLogIndex || mainLog.lastTerm() < arg.lastLogTerm) {
            std::unique_lock voteLock {m};
            votedFor = arg.candidateId;
            lastHeartbeat = std::chrono::steady_clock::now();
            voteLock.unlock();
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
    AppendEntriesReply reply;
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
    auto e = mainLog.at(arg.prevLogIndex);
    if (!e.has_value() || e.value().term == arg.prevLogTerm) {
        mainLog.merge(arg.entries);
        std::unique_lock commitLock{commitIndexMutex};
        if (arg.leaderCommit > commitIndex) {
            commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
        }
        commitLock.unlock();
        lastHeartbeat = std::chrono::steady_clock::now();
        reply.success = true;
        return reply;
    } else {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
}

template <typename Client>
void RaftImpl<Client>::appendEntries(){
    std::unique_lock appendEntriesLock {appendEntriesMutex};
    if (role != Role::Leader) {
        return;
    }
    std::vector<std::thread> threads;
    std::mutex threadsMutex{};
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
            [this, peerId, &threadsMutex] {
                auto& peer = peers.at(peerId);
                auto n = nextIndex.at(peerId);
                auto v = mainLog.at(n);
                auto g = mainLog.suffix(v.has_value()? v.value().index : mainLog.lastIndex());
                proto::AppendEntriesArg arg;
                arg.set_term(currentTerm);
                arg.set_leaderid(selfId);
                auto prevLogIndex = g.firstIndex();
                if (prevLogIndex <= 1) {
                    arg.set_prevlogindex(0);
                    arg.set_prevlogterm(0);
                } else {
                    arg.set_prevlogindex(prevLogIndex);
                    auto t = g.at(prevLogIndex - 1);
                    if (!t.has_value()) {
                        throw std::runtime_error {"cannot set prevLogTerm"};
                    }
                    arg.set_prevlogterm(t.value().term);
                }
                arg.set_leadercommit(commitIndex);
                for (const auto& eg : g.entries) {
                    auto e = arg.add_entries();
                    e->set_index(eg.index);
                    e->set_term(eg.term);
                    e->set_command(eg.command->serialize());
                }
                proto::AppendEntriesReply reply;
                auto status = peer->call(
                    "appendEntries",
                    &proto::Raft::Stub::appendEntries,
                    arg,
                    reply
                );
                std::lock_guard l{threadsMutex};
                if (role != Role::Leader) {
                    return;
                }
                if (status.has_value()) {
                    if (reply.success()) {
                        nextIndex[peerId] = g.lastIndex() + 1;
                        matchIndex[peerId] = g.lastIndex();
                    } else {
                        if (reply.term() > currentTerm) {
                            currentTerm = reply.term();
                            role = Role::Follower;
                        } else {
                            if (nextIndex[peerId] > 1) {
                                --nextIndex[peerId];
                            } else {
                                throw std::runtime_error {"Failed to commit empty log! "};
                            }
                        }
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
void RaftImpl<Client>::requestVote(){
    std::unique_lock voteLock{m};
    auto d = std::chrono::steady_clock::now() - lastHeartbeat;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(d) < std::chrono::milliseconds(time)) {
        return;
    }
    if (role == Role::Leader) {
        return;
    }
    role = Role::Candidate;
    ++currentTerm;
    votedFor = selfId;
    lastHeartbeat = std::chrono::steady_clock::now();
    std::atomic<int> votesGranted = 1;
    std::atomic<int> votesDeclined = 0;
    std::atomic<int> peersDown = 0;
    std::vector<std::thread> threads;
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
            [this, peerId, &votesGranted, &votesDeclined, &peersDown] {
                auto& peer = peers.at(peerId);
                proto::RequestVoteArg arg;
                arg.set_term(currentTerm);
                arg.set_candidateid(selfId);
                arg.set_lastlogindex(mainLog.lastIndex());
                arg.set_lastlogterm(mainLog.lastTerm());
                proto::RequestVoteReply reply;
                auto status = peer->call(
                    "requestVote",
                    &proto::Raft::Stub::requestVote,
                    arg,
                    reply
                );
                if (status.has_value()) {
                    if (reply.votegranted()) {
                        ++votesGranted;
                    } else {
                        ++votesDeclined;
                        if (reply.term() > currentTerm){
                            role = Role::Follower;
                            votedFor.reset();
                            lastHeartbeat = std::chrono::steady_clock::now();
                            return;
                        }
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
    if (!votedFor.has_value() || votedFor.value() != selfId) {
        return;
    }
    if (votesGranted >= clusterSize / 2 + 1) {
        role = Role::Leader;
        for (const auto& [a, _] : peers) {
            nextIndex[a] = mainLog.lastIndex() + 1;
            matchIndex[a] = 0;
        }
        appendEntries();
    }
}

template <typename Client>
bool RaftImpl<Client>::start(Command* command) {
    std::unique_lock startLock{m};
    if (role != Role::Leader) {
        return false;
    }
    LogEntry e {
        nextIndex[selfId]++,
        currentTerm,
        command
    };
    mainLog.append(e);
    return true;
}

template <typename Client>
void RaftImpl<Client>::kill(){
    std::unique_lock killLock {m};
    killed = true;
    cv.notify_all();
}

template <typename Client>
Log* RaftImpl<Client>::makeLog() {
    return new Log(commandFactory);
}

template <typename Client>
Log& RaftImpl<Client>::log(){
    return mainLog;
}

template <typename Client>
RaftImpl<Client>::~RaftImpl() {
    for (auto& [_, peer] : peers) {
        delete peer;
    }
}

} // namespace raft

#endif // RAFT_IMPL_H
