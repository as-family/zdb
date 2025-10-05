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
#include <algorithm>
#include <string>
#include <vector>
#include "common/RetryPolicy.hpp"
#include <unordered_map>
#include "raft/AsyncTimer.hpp"
#include <atomic>
#include <functional>
#include "common/FullJitter.hpp"
#include <queue>
#include <ranges>
#include "storage/Persister.hpp"
#include <thread>
#include <mutex>
#include <chrono>
#include <utility>
#include <memory>
#include <optional>
#include <condition_variable>

namespace raft {
template <typename Client>
class RaftImpl : public Raft {
public:
    RaftImpl(
        std::vector<std::string> p,
        const std::string& s,
        Channel<std::shared_ptr<raft::Command>>& c,
        const zdb::RetryPolicy& r,
        std::function<Client&(std::string, zdb::RetryPolicy, std::atomic<bool>& sc)> g,
        zdb::Persister& pers);
    void appendEntries(bool heartBeat) override;
    void requestVote() override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    bool start(std::shared_ptr<Command> command) override;
    void kill() override;
    Log& log() override;
    void cleanUpThreads();
    void persist() override;
    void readPersist(PersistentState) override;
    ~RaftImpl() override;
private:
    void applyCommittedEntries();
    std::recursive_mutex m{};
    std::atomic<std::chrono::steady_clock::rep> time {};
    Channel<std::shared_ptr<raft::Command>>& stateMachine;
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
    std::atomic<bool> stopCalls{false};
    zdb::Persister& persister;
};

template <typename Client>
RaftImpl<Client>::RaftImpl(
    std::vector<std::string> p,
    const std::string& s,
    Channel<std::shared_ptr<raft::Command>>& c,
    const zdb::RetryPolicy& r,
    std::function<Client&(std::string, zdb::RetryPolicy, std::atomic<bool>& sc)> g,
    zdb::Persister& pers)
: stateMachine {c},
  policy {r},
  persister {pers} {
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
        peers.emplace(peer, std::ref(g(peer, policy, stopCalls)));
    }
    heartbeatInterval = 30 * policy.rpcTimeout;
    electionTimeout = 3 * heartbeatInterval;
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
    killed = true;
    stopCalls = true;
    threadsCleanupTimer.stop();
    electionTimer.stop();
    heartbeatTimer.stop();
    std::unique_lock lock{m};
    while (!activeThreads.empty()) {
        std::thread& t = activeThreads.front();
        if (t.joinable()) {
            t.join();
        }
        activeThreads.pop();
    }
}

template <typename Client>
void RaftImpl<Client>::cleanUpThreads() {
    std::unique_lock lock{m};
    if (activeThreads.size() <= clusterSize * 3) {
        return;
    }
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
        persist();
    }
    if ((votedFor.has_value() && votedFor.value() == arg.candidateId) || !votedFor.has_value()) {
        if ((mainLog.lastTerm() == arg.lastLogTerm && mainLog.lastIndex() <= arg.lastLogIndex) || mainLog.lastTerm() < arg.lastLogTerm) {
            votedFor = arg.candidateId;
            lastHeartbeat = std::chrono::steady_clock::now();
            reply.term = currentTerm;
            reply.voteGranted = true;
            persist();
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
        persist();
    }
    auto e = mainLog.at(arg.prevLogIndex);
    if (arg.prevLogIndex == 0 || (e.has_value() && e.value().term == arg.prevLogTerm)) {
        bool mergeSuccess = mainLog.merge(arg.entries);
        if (!mergeSuccess) {
            reply.term = currentTerm;
            reply.success = false;
            reply.conflictIndex = mainLog.lastIndex() + 1;
            reply.conflictTerm = 0;
            return reply;
        }
        if (arg.leaderCommit > commitIndex) {
            commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
            applyCommittedEntries();
        }
        persist();
        reply.term = currentTerm;
        reply.success = true;
        return reply;
    } else {
        reply.term = currentTerm;
        reply.success = false;
        if (e.has_value()) {
            reply.conflictIndex = e.value().index;
            reply.conflictTerm = e.value().term;
        } else if (mainLog.lastIndex() > 0) {
            reply.conflictIndex = mainLog.lastIndex();
            reply.conflictTerm = mainLog.lastTerm();
        } else {
            reply.conflictIndex = 0;
            reply.conflictTerm = 0;
        }
        return reply;
    }
}

template <typename Client>
void RaftImpl<Client>::applyCommittedEntries() {
    while (lastApplied < commitIndex) {
        int i = lastApplied + 1;
        auto c = mainLog.at(i);
        if (!stateMachine.sendUntil(c.value().command, std::chrono::system_clock::now() + policy.rpcTimeout)) {
            break;
        }
        lastApplied = i;
    }
}

template <typename Client>
void RaftImpl<Client>::appendEntries(const bool heartBeat){
    if (killed.load()) {
        return;
    }
    std::unique_lock lock{m};
    if (heartBeat && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastHeartbeat) < heartbeatInterval) {
        return;
    }
    if (role != Role::Leader) {
        return;
    }
    lastHeartbeat = std::chrono::steady_clock::now();
    stopCalls = false;
    std::vector<std::thread> threads;
    successCount = 1;
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
        [this, peerId] {
                auto& peer = peers.at(peerId).get();
                auto n = nextIndex.at(peerId);
                auto g = mainLog.suffix(n);
                auto prevLogIndex = n == 0? 0 : n - 1;
                AppendEntriesArg arg {
                    selfId,
                    currentTerm,
                    prevLogIndex,
                    prevLogIndex == 0? 0 : mainLog.at(prevLogIndex).value().term,
                    commitIndex,
                    g
                };
                auto reply = peer.call(
                    "appendEntries",
                    arg
                );
                if (killed.load() || role != Role::Leader) {
                    stopCalls = true;
                    return;
                }
                std::lock_guard l{appendEntriesMutex};
                if (reply.has_value()) {
                    if (reply.value().success) {
                        if (g.lastIndex() != 0) {
                            nextIndex[peerId] = g.lastIndex() + 1;
                            matchIndex[peerId] = g.lastIndex();
                        }
                        ++successCount;
                        if (successCount >= clusterSize / 2 + 1) {
                            for (auto i = mainLog.lastIndex(); i > commitIndex; --i) {
                                if (mainLog.at(i).value().term == currentTerm) {
                                    auto matches = 1;
                                    for (auto& index : matchIndex | std::views::values) {
                                        if (index >= i) {
                                            ++matches;
                                        }
                                    }
                                    if (matches >= clusterSize / 2 + 1) {
                                        commitIndex = i;
                                        break;
                                    }
                                }
                            }
                            applyCommittedEntries();
                        }
                    } else if (reply.value().term > currentTerm) {
                        currentTerm = reply.value().term;
                        role = Role::Follower;
                        persist();
                        stopCalls = true;
                    } else {
                        if (mainLog.termFirstIndex(reply.value().conflictTerm) == 0) {
                            nextIndex[peerId] = std::max(reply.value().conflictIndex, static_cast<uint64_t>(1));
                        } else {
                            nextIndex[peerId] = mainLog.termFirstIndex(reply.value().conflictTerm);
                        }
                        nextIndex[peerId] = std::clamp(nextIndex[peerId], static_cast<uint64_t>(1), mainLog.lastIndex() + 1);
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
    persist();
    lastHeartbeat = std::chrono::steady_clock::now();
    votesGranted = 1;
    stopCalls = false;
    std::vector<std::thread> threads;
    for (auto& [peerId, _] : peers) {
        threads.emplace_back(
            [this, peerId] {
                auto& peer = peers.at(peerId).get();
                RequestVoteArg arg{
                    selfId,
                    currentTerm,
                    mainLog.lastIndex(),
                    mainLog.lastTerm()
                };
                auto reply = peer.call(
                    "requestVote",
                    arg
                );
                std::lock_guard l{requestVoteMutex};
                if (killed.load() || role != Role::Candidate) {
                    stopCalls = true;
                    return;
                }
                if (reply.has_value()) {
                    if (reply.value().voteGranted) {
                        ++votesGranted;
                        if (votesGranted == clusterSize / 2 + 1) {
                            role = Role::Leader;
                            for (const auto& [a, _] : peers) {
                                nextIndex[a] = mainLog.lastIndex() + 1;
                                matchIndex[a] = 0;
                            }
                            appendEntries(false);
                        }
                    } else if (reply.value().term > currentTerm) {
                        currentTerm = reply.value().term;
                        role = Role::Follower;
                        votedFor.reset();
                        persist();
                        stopCalls = true;
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
}

template <typename Client>
bool RaftImpl<Client>::start(std::shared_ptr<Command> command) {
    if (killed.load()) {
        stopCalls = true;
        return false;
    }
    std::unique_lock lock{m};
    if (role != Role::Leader) {
        stopCalls = true;
        return false;
    }
    command->term = currentTerm;
    command->index = mainLog.lastIndex() + 1;
    LogEntry e {
        mainLog.lastIndex() + 1,
        currentTerm,
        command
    };
    mainLog.append(e);
    persist();
    appendEntries(false);
    return true;
}

template <typename Client>
void RaftImpl<Client>::persist() {
    persister.save(PersistentState {
        currentTerm,
        votedFor,
        Log {mainLog.data()}
    });
}

template <typename Client>
void RaftImpl<Client>::readPersist(PersistentState s) {
    std::cerr << "readPersist" << std::endl;
    currentTerm = s.currentTerm;
    votedFor = s.votedFor;
    mainLog.clear();
    mainLog.merge(s.log);
}


template <typename Client>
void RaftImpl<Client>::kill(){
    killed = true;
    stopCalls = true;
}

template <typename Client>
Log& RaftImpl<Client>::log(){
    std::unique_lock lock{m};
    return mainLog;
}

} // namespace raft

#endif // RAFT_IMPL_H
