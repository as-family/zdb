// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef RAFT_IMPL_H
#define RAFT_IMPL_H

#include "raft/Raft.hpp"
#include "raft/Types.hpp"
#include "raft/Channel.hpp"
#include "raft/Log.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include "common/RetryPolicy.hpp"
#include <unordered_map>
#include "raft/AsyncTimer.hpp"
#include <atomic>
#include "proto/types.pb.h"
#include <functional>
#include "common/FullJitter.hpp"
#include <queue>

namespace raft {
template <typename Client>
class RaftImpl : public Raft {
public:
    RaftImpl(std::vector<std::string> p, const std::string& s, Channel<std::unique_ptr<raft::Command>>& c, const zdb::RetryPolicy& r, std::function<Client&(std::string, zdb::RetryPolicy)> g);
    void appendEntries(bool heartBeat) override;
    void requestVote() override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    bool start(std::unique_ptr<Command> command) override;
    void kill() override;
    Log& log() override;
    void cleanUpThreads();
    ~RaftImpl() override;
private:
    void applyCommittedEntries();
    std::mutex m{};
    std::atomic<std::chrono::steady_clock::rep> time {};
    Channel<std::unique_ptr<raft::Command>>& stateMachine;
    zdb::RetryPolicy policy;
    zdb::FullJitter fullJitter;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat;
    Log mainLog;
    std::atomic<bool> killed{false};
    std::unordered_map<std::string, std::reference_wrapper<Client>> peers;
    std::chrono::milliseconds threadsCleanupInterval;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
    AsyncTimer threadsCleanupTimer;
    int successCount{0};
    int votesGranted{0};
    std::queue<std::thread> activeThreads{};
    std::mutex appendEntriesMutex{};
    std::mutex requestVoteMutex{};
};

template <typename Client>
RaftImpl<Client>::RaftImpl(std::vector<std::string> p, const std::string& s, Channel<std::unique_ptr<raft::Command>>& c, const zdb::RetryPolicy& r, std::function<Client&(std::string, zdb::RetryPolicy)> g)
: stateMachine {c},
  policy {r} {
    selfId = s;
    clusterSize = p.size();
    nextIndex = std::unordered_map<std::string, uint64_t>{};
    matchIndex = std::unordered_map<std::string, uint64_t>{};
    for (const auto& a : p) {
        nextIndex[a] = 1;
        matchIndex[a] = 0;
    }
    p.erase(std::ranges::remove(p, selfId).begin(), p.end());
    for (const auto& peer : p) {
        peers.emplace(peer, std::ref(g(peer, policy)));
    }
    heartbeatInterval = 5 * policy.rpcTimeout;
    electionTimeout = 10 * heartbeatInterval;
    threadsCleanupInterval = heartbeatInterval;
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
    threadsCleanupTimer.start(
        [this] -> std::chrono::milliseconds {
            return threadsCleanupInterval + std::chrono::duration_cast<std::chrono::milliseconds>(fullJitter.jitter(
                    std::chrono::duration_cast<std::chrono::microseconds>(threadsCleanupInterval)));
        },
        [this] {
            cleanUpThreads();
        }
    );
}

template <typename Client>
RaftImpl<Client>::~RaftImpl() {
    // std::cerr << selfId << " is being destroyed\n";
    killed = true;
    threadsCleanupTimer.stop();
    electionTimer.stop();
    // std::cerr << selfId << " election timer stopped\n";
    heartbeatTimer.stop();
    // std::cerr << selfId << " heartbeat timer stopped\n";
    // std::cerr << selfId << " acquiring lock to stop RPC clients\n";
    std::unique_lock lock{m};
    // std::cerr << selfId << " threads cleanup timer stopped\n";
    // for (auto& [p, peer] : peers) {
    //     peer.get().stop();
    // }
    // std::cerr << selfId << " stopped all RPC clients\n";
    // std::cerr << selfId << " waiting for " << activeThreads.size() << " active threads to finish\n";
    while (!activeThreads.empty()) {
        std::thread& t = activeThreads.front();
        if (t.joinable()) {
            t.join();
        }
        activeThreads.pop();
    }
    // std::cerr << selfId << " all threads joined\n";
}

template <typename Client>
void RaftImpl<Client>::cleanUpThreads() {
    std::unique_lock lock{m};
    if (activeThreads.size() <= clusterSize * 3) {
        return;
    }
    // std::cerr << selfId << " cleaning up threads, activeThreads size: " << activeThreads.size() << "\n";
    std::vector<std::thread> threads;
    while (activeThreads.size() > clusterSize * 3) {
        std::thread& t = activeThreads.front();
        threads.push_back(std::move(t));
        activeThreads.pop();
    }
    lock.unlock();
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

template <typename Client>
RequestVoteReply RaftImpl<Client>::requestVoteHandler(const RequestVoteArg& arg) {
    if (killed.load()) {
        return RequestVoteReply{false, currentTerm};
    }
    std::unique_lock lock{m};
    // std::cerr << selfId << " received RequestVote from " << arg.candidateId << " for term " << arg.term << " (current term: " << currentTerm << ")\n";
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
    if (killed.load()) {
        return AppendEntriesReply{false, currentTerm};
    }
    std::unique_lock lock{m};
    if (arg.term >= currentTerm) {
        lastHeartbeat = std::chrono::steady_clock::now();
    }
    // std::cerr << selfId << " received AppendEntries from " << arg.leaderId << " for term " << arg.term << " (current term: " << currentTerm << ")\n";
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
        applyCommittedEntries(stateMachine);
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
void RaftImpl<Client>::applyCommittedEntries() {
    while (lastApplied < commitIndex) {
        ++lastApplied;
        auto c = mainLog.at(lastApplied);
        if (!stateMachine.sendUntil(std::move(c.value().command), std::chrono::system_clock::now() + policy.rpcTimeout)) {
            --lastApplied;
            break;
        }
    }
}

template <typename Client>
void RaftImpl<Client>::appendEntries(bool heartBeat){
    if (killed.load()) {
        return;
    }
    std::unique_lock lock{m, std::defer_lock};
    if (heartBeat) {
        lock.lock();
    }
    if (role != Role::Leader) {
        return;
    }
    std::vector<std::thread> threads;
    successCount = 1;
    // std::cerr << selfId << " is sending AppendEntries for term " << currentTerm << " (commitIndex: " << commitIndex << ", lastIndex: " << mainLog.lastIndex() << ")\n";
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
        [this, peerId] {
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
                if (killed.load()) {
                    return;
                }
                if (role != Role::Leader) {
                    return;
                }
                std::lock_guard l{appendEntriesMutex};
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
                            applyCommittedEntries(stateMachine);
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
    for (auto& t : threads) {
        if(t.joinable()) {
            activeThreads.push(std::move(t));
        }
    }
    if(heartBeat && activeThreads.size() > clusterSize * 10) {
        lock.unlock();
        cleanUpThreads();
    }
}

template <typename Client>
void RaftImpl<Client>::requestVote(){
    if (killed.load()) {
        return;
    }
    std::unique_lock lock{m};
    if (role == Role::Leader) {
        return;
    }
    if (const auto d = std::chrono::steady_clock::now() - lastHeartbeat; std::chrono::duration_cast<std::chrono::milliseconds>(d) < std::chrono::milliseconds{time.load()}) {
        return;
    }
    role = Role::Candidate;
    ++currentTerm;
    votedFor = selfId;
    lastHeartbeat = std::chrono::steady_clock::now();
    votesGranted = 1;
    // std::cerr << selfId << " is starting election for term " << currentTerm << "\n";
    std::vector<std::thread> threads;
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
            [this, peerId] {
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
                std::lock_guard l{requestVoteMutex};
                if (killed.load()) {
                    return;
                }
                if (role != Role::Candidate) {
                    return;
                }
                if (status.has_value()) {
                    if (reply.votegranted()) {
                        ++votesGranted;
                        if (votesGranted == clusterSize / 2 + 1) {
                            role = Role::Leader;
                            // std::cerr << selfId << " became leader for term " << currentTerm << "\n";
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
            activeThreads.push(std::move(t));
        }
    }
    if (activeThreads.size() > clusterSize * 10) {
        lock.unlock();
        cleanUpThreads();
    }
}

template <typename Client>
bool RaftImpl<Client>::start(std::unique_ptr<Command> command) {
    if (killed.load()) {
        return false;
    }
    std::unique_lock lock{m};
    if (role != Role::Leader) {
        return false;
    }
    LogEntry e {
        mainLog.lastIndex() + 1,
        currentTerm,
        (std::move(command))
    };
    mainLog.append(e);
    appendEntries(false);
    return true;
}

template <typename Client>
void RaftImpl<Client>::kill(){
    killed = true;
}

template <typename Client>
Log& RaftImpl<Client>::log(){
    std::unique_lock lock{m};
    return mainLog;
}

} // namespace raft

#endif // RAFT_IMPL_H
