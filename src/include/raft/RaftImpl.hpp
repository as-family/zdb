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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fmt/ranges.h>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <common/Command.hpp>
#include <spdlog/spdlog.h>
#include "common/FullJitter.hpp"
#include "common/RetryPolicy.hpp"
#include "raft/AsyncTimer.hpp"
#include "raft/Channel.hpp"
#include "raft/Log.hpp"
#include "raft/Raft.hpp"
#include "raft/Types.hpp"
#include "storage/Persister.hpp"

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
    bool hasVote(std::string candidateId);
    bool upToDate(uint64_t lastLogIndex, uint64_t lastLogTerm);
    std::mutex m;
    std::condition_variable electionCond;
    std::condition_variable appendCond;
    Channel<std::shared_ptr<raft::Command>>& stateMachine;
    zdb::RetryPolicy policy;
    zdb::FullJitter fullJitter;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat = std::chrono::steady_clock::now();
    Log mainLog;
    std::atomic<bool> killed{false};
    std::unordered_map<std::string, std::reference_wrapper<Client>> peers;
    AsyncTimer electionTimer;
    AsyncTimer heartbeatTimer;
    std::unordered_map<std::string, bool> shouldStartElection;
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
: Raft(s,  10 * r.rpcTimeout, 5 * 10 * r.rpcTimeout, p.size()),
  stateMachine {c},
  policy {r},
  persister {pers} {
    std::string peersList = fmt::format("{}", fmt::join(p, ", "));
    spdlog::info("{}: Init RaftImpl: peers: {}", selfId, peersList);
    std::erase(p, selfId);
    for (const auto& a : p) {
        nextIndex[a] = 1;
        matchIndex[a] = 0;
    }
    // RaftImpl<Client>::readPersist(persister.load());
    for (const auto& peer : p) {
        shouldStartHeartbeat.emplace(peer, false);
        appendNow.emplace(peer, false);
        shouldStartElection.emplace(peer, false);
        peers.emplace(peer, std::ref(g(peer, policy, stopCalls)));
    }
    electionTimer.start(
        [this] -> std::chrono::milliseconds {
            return electionTimeout +
                   std::chrono::duration_cast<std::chrono::milliseconds>(fullJitter.jitter(
                    std::chrono::duration_cast<std::chrono::microseconds>(electionTimeout)));
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
    for (const auto& peer : p) {
        leaderThreads.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(peer),
            std::forward_as_tuple(
                std::thread(&RaftImpl::requestVote, this, peer),
                std::thread(&RaftImpl::appendEntries, this, peer)
            )
        );
    }
    spdlog::info("{}: RaftImpl started: peers: {}", selfId, peersList);
}

template <typename Client>
RaftImpl<Client>::~RaftImpl() {
    spdlog::info("{}: ~RaftImpl", selfId);
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
    spdlog::info("{}: Destroyed RaftImpl", selfId);
}

template <typename Client>
RequestVoteReply RaftImpl<Client>::requestVoteHandler(const RequestVoteArg& arg) {
    std::unique_lock lk{m};
    if (killed.load()) {
        return RequestVoteReply{false, currentTerm};
    }
    spdlog::debug("{}: requestVoteHandler: term={} candidateId={} candidateTerm={}", selfId, currentTerm, arg.candidateId, arg.term);
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
        spdlog::info("{}: requestVoteHandler: became follower for term {}", selfId, currentTerm);
        appendCond.notify_all();
        electionCond.notify_all();
    }
    if (hasVote(arg.candidateId)) {
        if (upToDate(arg.lastLogIndex, arg.lastLogTerm)) {
            votedFor = arg.candidateId;
            role = Role::Follower;
            lastHeartbeat = std::chrono::steady_clock::now();
            reply.term = currentTerm;
            reply.voteGranted = true;
            persist();
            spdlog::debug("{}: requestVoteHandler: voted for {} for term {}", selfId, arg.candidateId, arg.term);
            return reply;
        }
    }
    reply.term = currentTerm;
    reply.voteGranted = false;
    return reply;
}

template <typename Client>
bool RaftImpl<Client>::hasVote(std::string candidateId) {
    return (votedFor.has_value() && votedFor.value() == candidateId) || !votedFor.has_value();
}

template <typename Client>
bool RaftImpl<Client>::upToDate(uint64_t lastLogIndex, uint64_t lastLogTerm) {
    return (mainLog.lastTerm() == lastLogTerm && mainLog.lastIndex() <= lastLogIndex) || mainLog.lastTerm() < lastLogTerm;
}

template <typename Client>
AppendEntriesReply RaftImpl<Client>::appendEntriesHandler(const AppendEntriesArg& arg) {
    std::unique_lock lock{m};
    if (killed.load()) {
        return AppendEntriesReply{false, currentTerm};
    }
    spdlog::debug("{}: appendEntriesHandler: term={} leader={} leaderTerm={} size={}",
        selfId, currentTerm, arg.leaderId, arg.term, arg.entries.data().size());
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
        role = Role::Follower;
        spdlog::info("{}: appendEntriesHandler: became follower for term {}", selfId, currentTerm);
        appendCond.notify_all();
        electionCond.notify_all();
    }
    lastHeartbeat = std::chrono::steady_clock::now();
    auto e = mainLog.at(arg.prevLogIndex);
    if (arg.prevLogIndex == 0 || (e.has_value() && e.value().term == arg.prevLogTerm)) {
        mainLog.merge(arg.entries);
        role = Role::Follower;
        appendCond.notify_all();
        electionCond.notify_all();
        persist();
        spdlog::info("{}: appendEntriesHandler: new lastIndex={}", selfId, mainLog.lastIndex());
        if (arg.leaderCommit > commitIndex) {
            commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
            applyCommittedEntries();
        }
        reply.term = currentTerm;
        reply.success = true;
        return reply;
    }
    reply.term = currentTerm;
    reply.success = false;
    if (e.has_value()) {
        reply.conflictIndex = mainLog.termFirstIndex(e.value().term);
        reply.conflictTerm = e.value().term;
    } else {
        reply.conflictIndex = mainLog.lastIndex() + 1;
        reply.conflictTerm = 0;
    }
    spdlog::debug("{}: appendEntriesHandler: conflict at prevLogIndex={}, prevLogTerm={}, conflictTerm={}, conflictIndex={}", selfId, arg.prevLogIndex, arg.prevLogTerm, reply.conflictTerm, reply.conflictIndex);
    return reply;
}

template <typename Client>
void RaftImpl<Client>::applyCommittedEntries() {
    spdlog::debug("{}: applyCommittedEntries: commitIndex={}, lastApplied={}", selfId, commitIndex, lastApplied);
    while (lastApplied < commitIndex) {
        uint64_t i = lastApplied + 1;
        auto c = mainLog.at(i);
        if (!stateMachine.sendUntil(c->command, std::chrono::system_clock::now() + policy.rpcTimeout)) {
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
        spdlog::debug("{}: appendEntries: to {} term={} nextIndex={}", selfId, peerId, currentTerm, nextIndex.at(peerId));
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
            appendNow[peerId] = false;
            shouldStartHeartbeat[peerId] = false;
            appendCond.notify_all();
            electionCond.notify_all();
            continue;
        }
        if (!reply.has_value()) {
            spdlog::debug("{}: appendEntries: no Response: {}", selfId, peerId);
            appendNow[peerId] = true;
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
            spdlog::info("{}: appendEntries: to {} became follower for term {}", selfId, peerId, currentTerm);
            appendCond.notify_all();
            electionCond.notify_all();
            continue;
        }
        if (reply.value().term < currentTerm || arg.term != currentTerm) {
            appendNow[peerId] = true;
            continue;
        }
        if (reply.value().success) {
            nextIndex[peerId] = n + g.data().size();
            matchIndex[peerId] = nextIndex[peerId] - 1;
            for (auto i = mainLog.lastIndex(); i > commitIndex && mainLog.at(i).value().term == currentTerm; --i) {
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
            }
            appendNow[peerId] = false;
            shouldStartHeartbeat[peerId] = false;
            appendCond.notify_all();
        } else {
            if (mainLog.termLastIndex(reply.value().conflictTerm) != 0) {
                nextIndex[peerId] = mainLog.termLastIndex(reply.value().conflictTerm) + 1;
            } else {
                nextIndex[peerId] = reply.value().conflictIndex;
            }
            nextIndex[peerId] = std::clamp(nextIndex[peerId], static_cast<uint64_t>(1), mainLog.lastIndex() + 1);
            appendNow[peerId] = true;
        }
    }
    spdlog::debug("{}: appendEntries thread to {} exiting", selfId, peerId);
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
    spdlog::info("{}: initVote: starting election for term {}", selfId, currentTerm + 1);
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
    for (auto& e : shouldStartElection | std::views::values) {
        e = true;
    }
    electionCond.notify_all();
}

template <typename Client>
void RaftImpl<Client>::requestVote(std::string peerId) {
    auto& peer = peers.at(peerId).get();
    while (!killed.load()) {
        std::unique_lock initLock{m};
        electionCond.wait(initLock, [this, peerId] { return killed.load() || (role == Role::Candidate && shouldStartElection.at(peerId)); });
        if (killed.load()) {
            break;
        }
        if (role != Role::Candidate) {
            continue;
        }
        if (!shouldStartElection.at(peerId)) {
            continue;
        }
        spdlog::debug("{}: requestVote: to {} term={}", selfId, peerId, currentTerm);
        shouldStartElection[peerId] = false;
        auto arg = requestVoteArg;
        initLock.unlock();
        auto reply = peer.call(
            "requestVote",
            arg
        );

        std::unique_lock resLock{m};
        if (killed.load() || role != Role::Candidate) {
            stopCalls = true;
            continue;
        }
        if (!reply.has_value()) {
            spdlog::warn("{}: requestVote: no Response: {}", selfId, peerId);
            continue;
        }
        if (reply.value().term > currentTerm) {
            stopCalls = true;
            currentTerm = reply.value().term;
            role = Role::Follower;
            votedFor.reset();
            persist();
            lastHeartbeat = std::chrono::steady_clock::now();
            spdlog::info("{}: requestVote: became follower for term {}", selfId, currentTerm);
            electionCond.notify_all();
            continue;
        }
        if (reply.value().term < currentTerm || arg.term != currentTerm) {
            continue;
        }
        if (reply.value().voteGranted) {
            ++votesGranted;
            if (votesGranted >= clusterSize / 2 + 1) {
                spdlog::info("{}: requestVote: became Leader for term {}", selfId, currentTerm);
                role = Role::Leader;
                stopCalls = true;
                for (const auto& a : peers | std::views::keys) {
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
    if (killed.load() || role != Role::Leader) {
        stopCalls = true;
        appendCond.notify_all();
        electionCond.notify_all();
        return false;
    }
    command->term = currentTerm;
    command->index = mainLog.lastIndex() + 1;
    spdlog::info("{}: start: command term={} index={}", selfId, command->term, command->index);
    LogEntry e {
        command->index,
        command->term,
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
    std::unique_lock lock{m};
    currentTerm = s.currentTerm;
    votedFor = s.votedFor;
    mainLog.clear();
    mainLog.merge(s.log);
}

template <typename Client>
void RaftImpl<Client>::kill(){
    killed = true;
    stopCalls = true;
    appendCond.notify_all();
    electionCond.notify_all();
}

template <typename Client>
Log& RaftImpl<Client>::log(){
    std::unique_lock lock{m};
    return mainLog;
}

} // namespace raft

#endif // RAFT_IMPL_H
