#include "RaftImpl.hpp"
#include <vector>
#include <string>
#include "raft/Channel.hpp"
#include "raft/Command.hpp"
#include "raft/Log.hpp"
#include <algorithm>
#include "common/RPCService.hpp"
#include <chrono>
#include <tuple>
#include "proto/raft.pb.h"
#include <algorithm>

namespace raft {

RaftImpl::RaftImpl(std::vector<std::string> p, std::string s, Channel& c, zdb::RetryPolicy r, Command* (*f)(const std::string&))
    : serviceChannel {c},
      policy {r},
      fullJitter {},
      electionTimer {},
      heartbeatTimer {},
      lastHeartbeat {std::chrono::steady_clock::now()},
      commandFactory {f},
      mainLog {f} {
    selfId = s;
    clusterSize = p.size();
    nextIndex = std::unordered_map<std::string, uint64_t>{};
    matchIndex = std::unordered_map<std::string, uint64_t>{};
    for (const auto a : p) {
        nextIndex[a] = 1;
        matchIndex[a] = 0;
    }
    p.erase(std::remove(p.begin(), p.end(), selfId), p.end());
    for (const auto peer : p) {
        peers.emplace(std::piecewise_construct,
                       std::forward_as_tuple(peer),
                       std::forward_as_tuple(peer, policy));
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

RequestVoteReply RaftImpl::requestVoteHandler(const RequestVoteArg& arg) {
    RequestVoteReply reply;
    if (arg.term >= currentTerm) {
        std::unique_lock termLock {m};
        currentTerm = arg.term;
        role = Role::Follower;
        cv.notify_all();
        termLock.unlock();
        if (votedFor.has_value() && votedFor.value() == arg.candidateId || !votedFor.has_value()) {
            if (mainLog.lastTerm() == arg.lastLogTerm && mainLog.lastIndex() <= arg.lastLogIndex || mainLog.lastTerm() < arg.lastLogTerm) {
                std::unique_lock voteLock {m};
                votedFor = arg.candidateId;
                reply.voteGranted = true;
                lastHeartbeat = std::chrono::steady_clock::now();
                voteLock.unlock();
                return reply;
            }
        }
    }
    reply.term = currentTerm;
    reply.voteGranted = false;
    return reply;
}

AppendEntriesReply RaftImpl::appendEntriesHandler(const AppendEntriesArg& arg) {
    AppendEntriesReply reply;
    if (arg.term >= currentTerm) {
        std::unique_lock termLock {m};
        currentTerm = arg.term;
        role = Role::Follower;
        lastHeartbeat = std::chrono::steady_clock::now();
        cv.notify_all();
        termLock.unlock();
        auto e = mainLog.at(arg.prevLogIndex);
        if (!e.has_value() || e.value().term == arg.prevLogTerm) {
            mainLog.merge(arg.entries);
            std::unique_lock commitLock{commitIndexMutex};
            if (arg.leaderCommit > commitIndex) {
                commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
            }
            commitLock.unlock();
            reply.success = true;
            return reply;
        }
    }
    reply.term = currentTerm;
    reply.success = false;
    return reply;
}

void RaftImpl::appendEntries(){
    std::unique_lock appendEntriesLock {appendEntriesMutex};
    if (role != Role::Leader) {
        return;
    }
    std::vector<std::thread> threads;
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
            [this, peerId] {
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
                auto status = peer.call(
                    "appendEntries",
                    &proto::Raft::Stub::appendEntries,
                    arg,
                    reply
                );
                if (status.has_value()) {
                    if (reply.success()) {
                        nextIndex[peerId] = g.lastIndex();
                        matchIndex[peerId] = g.lastIndex();
                    } else {
                        if (reply.term() > currentTerm) {
                            std::unique_lock termLock{m};
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
                } else {
                    if (!peer.connected()) {
                        return;
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

void RaftImpl::requestVote(){
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
                auto status = peer.call(
                    "requestVote",
                    &proto::Raft::Stub::requestVote,
                    arg,
                    reply
                );
                if (status.has_value()) {
                    if (reply.votegranted()) {
                        ++votesGranted;
                    } else {
                        if (reply.term() > currentTerm){
                            role = Role::Follower;
                            votedFor.reset();
                            lastHeartbeat = std::chrono::steady_clock::now();
                            return;
                        } else {
                            ++votesDeclined;
                        }
                    }
                } else {
                    if (!peer.connected()) {
                        ++peersDown;
                        return;
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
        appendEntries();
    }
}

bool RaftImpl::start(Command* command) {
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

void RaftImpl::kill(){
    std::unique_lock killLock {m};
    killed = true;
    cv.notify_all();
}
Log* RaftImpl::makeLog() {
    return new Log(commandFactory);
}
Log& RaftImpl::log(){
    return mainLog;
}
void RaftImpl::connectPeers() {
    for (auto& p : peers) {
        if (!p.second.available()) {
            throw std::runtime_error {selfId + ": no connection to " + p.first};
        }
    }
}
RaftImpl::~RaftImpl() {
}


} // namespace raft
