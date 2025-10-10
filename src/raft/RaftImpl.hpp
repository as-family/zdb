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
#include <print>

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
    void appendEntries(std::string peerId) override;
    void requestVote(std::string peerId) override;
    AppendEntriesReply appendEntriesHandler(const AppendEntriesArg& arg) override;
    RequestVoteReply requestVoteHandler(const RequestVoteArg& arg) override;
    bool start(std::shared_ptr<Command> command) override;
    void kill() override;
    Log& log() override;
    void persist() override;
    void readPersist(PersistentState) override;
    ~RaftImpl() override;
private:
    void initVote();
    void applyCommittedEntries();
    std::mutex m;
    std::condition_variable electionCond;
    std::condition_variable appendCond;
    std::atomic<std::chrono::steady_clock::rep> time;
    Channel<std::shared_ptr<raft::Command>>& stateMachine;
    zdb::RetryPolicy policy;
    zdb::FullJitter fullJitter;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat = std::chrono::steady_clock::now();
    Log mainLog;
    std::atomic<bool> killed{false};
    std::unordered_map<std::string, std::reference_wrapper<Client>> peers;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
    bool shouldStartElection {false};
    std::unordered_map<std::string, bool> shouldStartHeartbeat;
    std::unordered_map<std::string, bool> appendNow;
    int votesGranted{0};
    RequestVoteArg requestVoteArg {"", 0, 0, 0};
    std::unordered_map<std::string, std::pair<std::thread, std::thread>> leaderThreads;
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
        leaderThreads.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(peer),
            std::forward_as_tuple(
                std::thread(&RaftImpl::requestVote, this, peer),
                std::thread(&RaftImpl::appendEntries, this, peer)
            )
        );
        shouldStartHeartbeat.emplace(peer, false);
        appendNow.emplace(peer, false);
    }
    heartbeatInterval = 10 * policy.rpcTimeout;
    electionTimeout = 10 * heartbeatInterval;
    electionTimer.start(
        [this] -> std::chrono::milliseconds {
            auto t = electionTimeout +
                   std::chrono::duration_cast<std::chrono::milliseconds>(fullJitter.jitter(
                    std::chrono::duration_cast<std::chrono::microseconds>(electionTimeout)));
            time = t.count();
            return t;
        },
        [this] {
            initVote();
        }
    );
    heartbeatTimer.start(
        [this] -> std::chrono::milliseconds {
            return heartbeatInterval;
        },
        [this] {
            if (role == Role::Leader) {
                for (auto& shouldStart : shouldStartHeartbeat | std::views::values) {
                    shouldStart = true;
                }
                appendCond.notify_all();
            }
        }
    );
}

template <typename Client>
RaftImpl<Client>::~RaftImpl() {
    killed = true;
    stopCalls = true;
    appendCond.notify_all();
    electionCond.notify_all();
    heartbeatTimer.stop();
    electionTimer.stop();
    for (auto& th : leaderThreads | std::views::values) {
        if (th.first.joinable()) {
            th.first.join();
        }
        if (th.second.joinable()) {
            th.second.join();
        }
    }
}

template <typename Client>
RequestVoteReply RaftImpl<Client>::requestVoteHandler(const RequestVoteArg& arg) {
    std::unique_lock lock{m};
    if (killed.load()) {
        return RequestVoteReply{false, currentTerm};
    }
    std::println(stderr, "{} requestVoteHandler: {} {} . {} {}", std::chrono::system_clock::now(), selfId, currentTerm, arg.candidateId, arg.term);
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
            std::println(stderr, "{} VotedFor: {} {} . {} {}", std::chrono::system_clock::now(), selfId, currentTerm, arg.candidateId, arg.term);
            votedFor = arg.candidateId;
            role = Role::Follower;
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
    std::unique_lock lock{m};
    if (killed.load()) {
        return AppendEntriesReply{false, currentTerm};
    }
    std::println(stdout, "{} appendEntriesHandler: {} {} . {} {}", std::chrono::system_clock::now(), selfId, currentTerm, arg.leaderId, arg.term);
    AppendEntriesReply reply;
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
    if (arg.term > currentTerm) {
        currentTerm = arg.term;
        votedFor.reset();
        persist();
    }
    role = Role::Follower;
    lastHeartbeat = std::chrono::steady_clock::now();
    auto e = mainLog.at(arg.prevLogIndex);
    if (arg.prevLogIndex == 0 || (e.has_value() && e.value().term == arg.prevLogTerm)) {
        mainLog.merge(arg.entries);
        persist();
        if (arg.leaderCommit > commitIndex) {
            commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
            applyCommittedEntries();
        }
        reply.term = currentTerm;
        reply.success = true;
        std::println(stderr, "{} appended: {} {} {}", std::chrono::system_clock::now(), selfId, arg.leaderId, arg.term);
        return reply;
    } else {
        reply.term = currentTerm;
        reply.success = false;
        if (e.has_value()) {
            reply.conflictIndex = mainLog.termFirstIndex(e.value().term);
            reply.conflictTerm = e.value().term;
        } else {
            reply.conflictIndex = mainLog.lastIndex() + 1;
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
void RaftImpl<Client>::appendEntries(std::string peerId){
    auto& peer = peers.at(peerId).get();
    while (!killed.load()) {
        std::unique_lock initLock{m};
        std::println(stderr, "{} waiting: {} {}", std::chrono::system_clock::now(), selfId, peerId);
        appendCond.wait(initLock, [this, peerId] { return killed.load() || (role == Role::Leader && (appendNow.at(peerId) || shouldStartHeartbeat.at(peerId))); });
        if (killed.load()) {
            break;
        }
        if (role != Role::Leader) {
            continue;
        }
        if (!(appendNow.at(peerId) || shouldStartHeartbeat.at(peerId))) {
            continue;
        }
        std::println(stderr, "{} appendEntries: {} {}", std::chrono::system_clock::now(), selfId, peerId);
        stopCalls = false;
        auto n = nextIndex.at(peerId);
        auto g = mainLog.suffix(n);
        auto prevLogIndex = n - 1;
        AppendEntriesArg arg {
            selfId,
            currentTerm,
            prevLogIndex,
            prevLogIndex == 0? 0 : mainLog.at(prevLogIndex).value().term,
            commitIndex,
            g
        };
        initLock.unlock();
        auto reply = peer.call(
            "appendEntries",
            arg
        );
        std::unique_lock resLock{m};
        if (killed.load() || role != Role::Leader) {
            stopCalls = true;
            continue;
        }
        if (!reply.has_value()) {
            std::println(stderr, "{} no Response: {} {}", std::chrono::system_clock::now(), selfId, peerId);
            continue;
        }
        if (reply.value().term < currentTerm || arg.term != currentTerm) {
            continue;
        }
        if (reply.value().term > currentTerm) {
            currentTerm = reply.value().term;
            role = Role::Follower;
            votedFor.reset();
            persist();
            stopCalls = true;
            appendNow[peerId] = false;
            shouldStartHeartbeat[peerId] = false;
            lastHeartbeat = std::chrono::steady_clock::now();
            appendCond.notify_all();
            electionCond.notify_all();
            continue;
        }
        if (reply.value().success) {
            if (g.lastIndex() != 0) {
                nextIndex[peerId] = g.lastIndex() + 1;
                matchIndex[peerId] = g.lastIndex();
            }
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
                        applyCommittedEntries();
                        break;
                    }
                } else {
                    break;
                }
            }
            appendNow[peerId] = false;
            shouldStartHeartbeat[peerId] = false;
            appendCond.notify_all();
        } else {
            if (reply.value().conflictIndex != 0 && mainLog.termFirstIndex(reply.value().conflictTerm) != 0) {
                nextIndex[peerId] = mainLog.termLastIndex(reply.value().conflictTerm) + 1;
            } else {
                nextIndex[peerId] = reply.value().conflictIndex;
            }
            nextIndex[peerId] = std::clamp(nextIndex[peerId], static_cast<uint64_t>(1), mainLog.lastIndex() + 1);
        }
    }
}

template<typename Client>
void RaftImpl<Client>::initVote() {
    std::unique_lock initLock{m};
    if (role == Role::Leader) {
        return;
    }
    if (const auto d = std::chrono::steady_clock::now() - lastHeartbeat; std::chrono::duration_cast<std::chrono::milliseconds>(d) < electionTimeout) {
        return;
    }
    std::println(stderr, "{} initVote: {} {}", std::chrono::system_clock::now(), selfId, currentTerm);
    role = Role::Candidate;
    ++currentTerm;
    votedFor = selfId;
    persist();
    requestVoteArg = {
        selfId,
        currentTerm,
        mainLog.lastIndex(),
        mainLog.lastTerm()
    };
    votesGranted = 1;
    stopCalls = false;
    shouldStartElection = true;
    electionCond.notify_all();
}

template <typename Client>
void RaftImpl<Client>::requestVote(std::string peerId) {
    auto& peer = peers.at(peerId).get();
    while (!killed.load()) {
        std::unique_lock initLock{m};
        electionCond.wait(initLock, [this, peerId] { return killed.load() || (role == Role::Candidate && shouldStartElection); });
        if (killed.load()) {
            break;
        }
        if (role != Role::Candidate) {
            continue;
        }
        if (!shouldStartElection) {
            continue;
        }
        auto arg = requestVoteArg;
        initLock.unlock();
        auto reply = peer.call(
            "requestVote",
            arg
        );
        std::unique_lock resLock{m};
        if (killed.load() || role != Role::Candidate) {
            stopCalls = true;
            shouldStartElection = false;
            continue;
        }
        if (!reply.has_value()) {
            continue;
        }
        if (reply.value().term < currentTerm || arg.term != currentTerm) {
            continue;
        }
        if (reply.value().term > currentTerm) {
            shouldStartElection = false;
            stopCalls = true;
            currentTerm = reply.value().term;
            role = Role::Follower;
            votedFor.reset();
            persist();
            lastHeartbeat = std::chrono::steady_clock::now();
            electionCond.notify_all();
            continue;
        }
        if (reply.value().voteGranted) {
            ++votesGranted;
            if (votesGranted == clusterSize / 2 + 1) {
                std::println(stderr, "{} Leader: {} {}", std::chrono::system_clock::now(), selfId, currentTerm);
                role = Role::Leader;
                shouldStartElection = false;
                stopCalls = true;
                for (const auto& [a, _] : peers) {
                    nextIndex[a] = mainLog.lastIndex() + 1;
                    matchIndex[a] = 0;
                }
                for (auto& app : appendNow | std::views::values) {
                    app = true;
                }
                appendCond.notify_all();
                electionCond.notify_all();
            }
        }
    }
}

template <typename Client>
bool RaftImpl<Client>::start(std::shared_ptr<Command> command) {
    std::unique_lock lock{m};
    if (killed.load()) {
        stopCalls = true;
        return false;
    }
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
    for (auto& app : appendNow | std::views::values) {
        app = true;
    }
    appendCond.notify_all();
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
