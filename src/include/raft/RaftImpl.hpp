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
#include "common/Util.hpp"
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
    InstallSnapshotReply installSnapshotHandler(const InstallSnapshotArg& arg) override;
    bool start(std::shared_ptr<Command> command) override;
    void snapshot(const uint64_t index, const std::string& snapshotData) override;
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
    bool becameFollower(uint64_t term, std::string peerId);
    void asyncInstallSnapshot();
    std::mutex m;
    std::mutex applyMutex;
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
    std::thread snapshotThread;
    std::atomic<bool> stopCalls{false};
    zdb::Persister& persister;
    std::string snapshotData;
    std::atomic<bool> pendingSnapshot{false};
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
    snapshotThread = std::thread(&RaftImpl::asyncInstallSnapshot, this);
    RaftImpl<Client>::readPersist(persister.load());
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
    kill();
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
    if (snapshotThread.joinable()) {
        snapshotThread.join();
    }
    spdlog::info("{}: Destroyed RaftImpl", selfId);
}

template <typename Client>
void RaftImpl<Client>::asyncInstallSnapshot() {
    while (!killed.load()) {
        if (pendingSnapshot.load()) {
            std::string data;
            uint64_t index, term;
            {
                std::unique_lock lock{m};
                data = snapshotData;
                index = lastIncludedIndex;
                term = lastIncludedTerm;
            }
            auto uuid = generate_uuid_v7();
            if (stateMachine.sendUntil(std::make_shared<zdb::InstallSnapshotCommand>(uuid, index, term, data), std::chrono::system_clock::now() + policy.rpcTimeout)) {
                spdlog::info("{}: readPersist: applied snapshot asynchronously", selfId);
                pendingSnapshot.store(false);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

template <typename Client>
bool RaftImpl<Client>::becameFollower(uint64_t term, std::string peerId) {
    if (term > currentTerm) {
        currentTerm = term;
        role = Role::Follower;
        votedFor.reset();
        persist();
        lastHeartbeat = std::chrono::steady_clock::now();
        spdlog::info("{}: becameFollower: became follower for term {} leader {}", selfId, currentTerm, peerId);
        stopCalls = true;
        appendNow[peerId] = false;
        shouldStartHeartbeat[peerId] = false;
        appendCond.notify_all();
        electionCond.notify_all();
        return true;
    }
    return false;
}

template <typename Client>
RequestVoteReply RaftImpl<Client>::requestVoteHandler(const RequestVoteArg& arg) {
    std::unique_lock lk{m};
    if (killed.load() || pendingSnapshot.load()) {
        return RequestVoteReply{false, currentTerm};
    }
    spdlog::debug("{}: requestVoteHandler: term={} candidateId={} candidateTerm={}", selfId, currentTerm, arg.candidateId, arg.term);
    RequestVoteReply reply;
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.voteGranted = false;
        return reply;
    }
    becameFollower(arg.term, arg.candidateId);
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
    if (killed.load() || pendingSnapshot.load()) {
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
    becameFollower(arg.term, arg.leaderId);
    lastHeartbeat = std::chrono::steady_clock::now();
    auto e = mainLog.at(arg.prevLogIndex);
    if (arg.prevLogIndex == 0 || (e.has_value() && e.value().term == arg.prevLogTerm) || (!e.has_value() && arg.prevLogIndex == lastIncludedIndex && arg.prevLogTerm == lastIncludedTerm)) {
        mainLog.merge(arg.entries);
        role = Role::Follower;
        appendCond.notify_all();
        electionCond.notify_all();
        reply.term = currentTerm;
        reply.success = true;
        persist();
        spdlog::debug("{}: appendEntriesHandler: new lastIndex={}", selfId, mainLog.lastIndex());
        if (arg.leaderCommit > commitIndex) {
            commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
            lock.unlock();
            applyCommittedEntries();
        }
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
InstallSnapshotReply RaftImpl<Client>::installSnapshotHandler(const InstallSnapshotArg& arg) {
    std::unique_lock lock{m};
    InstallSnapshotReply reply;
    if (killed.load()) {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
    spdlog::debug("{}: installSnapshotHandler: term={} leader={} lastIncludedIndex={} lastIncludedTerm={}", selfId, currentTerm, arg.leaderId, arg.lastIncludedIndex, arg.lastIncludedTerm);
    if (arg.term < currentTerm) {
        reply.term = currentTerm;
        reply.success = false;
        return reply;
    }
    becameFollower(arg.term, arg.leaderId);
    lastHeartbeat = std::chrono::steady_clock::now();
    mainLog.trimPrefix(arg.lastIncludedIndex, arg.lastIncludedTerm);
    lastApplied = arg.lastIncludedIndex;
    commitIndex = arg.lastIncludedIndex;
    lastIncludedIndex = arg.lastIncludedIndex;
    lastIncludedTerm = arg.lastIncludedTerm;
    snapshotData = arg.data;
    persist();
    auto uuid = generate_uuid_v7();
    reply.term = currentTerm;
    reply.success = true;
    lock.unlock();
    if (!stateMachine.sendUntil(std::make_shared<zdb::InstallSnapshotCommand>(uuid, arg.lastIncludedIndex, arg.lastIncludedTerm, arg.data), std::chrono::system_clock::now() + policy.rpcTimeout)) {
        spdlog::error("{}: installSnapshotHandler: failed to apply snapshot lastIncludedIndex={}", selfId, arg.lastIncludedIndex);
        pendingSnapshot.store(true);
    }
    return reply;
}

template <typename Client>
void RaftImpl<Client>::applyCommittedEntries() {
    std::unique_lock applyLock{applyMutex};
    std::unique_lock lock1{m};
    if (lastApplied >= commitIndex || pendingSnapshot.load()) {
        return;
    }
    lock1.unlock();
    spdlog::debug("{}: applyCommittedEntries: commitIndex={}, lastApplied={}", selfId, commitIndex, lastApplied);
    while (true) {
        lock1.lock();
        if (lastApplied >= commitIndex) {
            break;
        }
        uint64_t i = lastApplied + 1;
        auto c = mainLog.at(i);
        if (!c.has_value()) {
            spdlog::error("{}: applyCommittedEntries: no log entry at index {}", selfId, i);
            break;
        }
        lock1.unlock();
        if (!stateMachine.sendUntil(c->command, std::chrono::system_clock::now() + policy.rpcTimeout)) {
            spdlog::error("{}: applyCommittedEntries: failed to apply command at index {}", selfId, i);
            break;
        }
        {
            std::unique_lock lock2{m};
            lastApplied = i;
        }
    }
}

template <typename Client>
void RaftImpl<Client>::appendEntries(std::string peerId){
    auto& peer = peers.at(peerId).get();
    bool sendSnapshot = false;
    while (!killed.load()) {
        std::unique_lock initLock{m};
        appendCond.wait(initLock, [this, peerId] { return killed.load() || (role == Role::Leader && (appendNow.at(peerId) || shouldStartHeartbeat.at(peerId))); });
        if (killed.load()) {
            break;
        }
        if (pendingSnapshot.load()) {
            continue;
        }
        if (role != Role::Leader) {
            continue;
        }
        if (!(appendNow.at(peerId) || shouldStartHeartbeat.at(peerId))) {
            continue;
        }
        spdlog::debug("{}: appendEntries: to {} term={} nextIndex={}", selfId, peerId, currentTerm, nextIndex.at(peerId));
        auto n = nextIndex.at(peerId);
        if (mainLog.firstIndex() > n) {
            spdlog::warn("{}: appendEntries: to {} nextIndex {} less than firstIndex {}", selfId, peerId, n, mainLog.firstIndex());
            sendSnapshot = true;
            n = mainLog.firstIndex();
        }
        auto g = mainLog.suffix(n);
        auto prevLogIndex = n - 1;
        auto mainLogAtPrev = mainLog.at(prevLogIndex);
        uint64_t prevLogTerm = 0;
        if (mainLogAtPrev.has_value()) {
            prevLogTerm = mainLogAtPrev.value().term;
        } else if (sendSnapshot || prevLogIndex <= lastIncludedIndex) {
            prevLogTerm = lastIncludedTerm;
        } else if (prevLogIndex != 0) {
            spdlog::error("{}: appendEntries: to {} prevLogIndex {} has no entry", selfId, peerId, prevLogIndex);
            appendNow[peerId] = true;
            continue;
        }
        AppendEntriesArg arg {
            selfId,
            currentTerm,
            prevLogIndex,
            prevLogTerm,
            commitIndex,
            g
        };
        stopCalls = false;
        if (sendSnapshot) {
            spdlog::debug("{}: appendEntries: sending InstallSnapshot to {}", selfId, peerId);
            auto buffer = persister.load().snapshotData;
            InstallSnapshotArg snapArg {
                selfId,
                currentTerm,
                lastIncludedIndex,
                lastIncludedTerm,
                buffer
            };
            initLock.unlock();
            auto snapReply = peer.call(
                "installSnapshot",
                snapArg
            );
            initLock.lock();
            if (killed.load() || role != Role::Leader) {
                stopCalls = true;
                appendNow[peerId] = false;
                shouldStartHeartbeat[peerId] = false;
                appendCond.notify_all();
                electionCond.notify_all();
                continue;
            }
            if (pendingSnapshot.load()) {
                continue;
            }
            if (snapReply.has_value()) {
                if (becameFollower(snapReply.value().term, peerId)) {
                    continue;
                }
                if (snapReply.value().term < currentTerm || snapArg.term != currentTerm) {
                    continue;
                }
                if (snapReply.value().success) {
                    nextIndex[peerId] = lastIncludedIndex + 1;
                    matchIndex[peerId] = lastIncludedIndex;
                    sendSnapshot = false;
                } else {
                    spdlog::warn("{}: appendEntries: InstallSnapshot to {} was not successful", selfId, peerId);
                    continue;
                }
            } else {
                spdlog::warn("{}: appendEntries: no InstallSnapshot Response from {}", selfId, peerId);
                continue;
            }
        }
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
        if (pendingSnapshot.load()) {
            continue;
        }
        if (!reply.has_value()) {
            spdlog::debug("{}: appendEntries: no Response: {}", selfId, peerId);
            appendNow[peerId] = true;
            continue;
        }
        if (becameFollower(reply.value().term, peerId)) {
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
                    resLock.unlock();
                    applyCommittedEntries();
                    resLock.lock();
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
    if (pendingSnapshot.load()) {
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
        if (pendingSnapshot.load()) {
            continue;
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
        if (pendingSnapshot.load()) {
            continue;
        }
        if (!reply.has_value()) {
            spdlog::warn("{}: requestVote: no Response: {}", selfId, peerId);
            continue;
        }
        if (becameFollower(reply.value().term, peerId)) {
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
    if (pendingSnapshot.load()) {
        return false;
    }
    command->term = currentTerm;
    command->index = std::max(mainLog.lastIndex() + 1, lastIncludedIndex + 1);
    spdlog::debug("{}: start: command term={} index={}", selfId, command->term, command->index);
    LogEntry e {
        command->index,
        command->term,
        command
    };
    mainLog.append(e);
    persist();
    for (auto& a : appendNow | std::views::values) {
        a = true;
    }
    appendCond.notify_all();
    return true;
}

template <typename Client>
void RaftImpl<Client>::snapshot(const uint64_t index, const std::string& sd) {
    std::unique_lock lock{m};
    if (killed.load()) {
        return;
    }
    if (pendingSnapshot.load()) {
        spdlog::warn("{}: snapshot: another snapshot is pending, skipping", selfId);
        return;
    }
    if (index < mainLog.firstIndex()) {
        spdlog::error("{}: snapshot: index {} <= firstIndex {}, no trimming needed", selfId, index, mainLog.firstIndex());
        return;
    }
    if (index > mainLog.lastIndex()) {
        spdlog::error("{}: snapshot: index {} > lastIndex {}, cannot trim", selfId, index, mainLog.lastIndex());
        return;
    }
    if (index == lastIncludedIndex) {
        snapshotData = sd;
        persist();
        spdlog::debug("{}: snapshot: refreshed snapshot data at index {}", selfId, index);
        return;
    }
    spdlog::debug("{}: snapshot: index={}", selfId, index);
    lastIncludedIndex = index;
    lastIncludedTerm = mainLog.at(index).value().term;
    mainLog.trimPrefix(index, lastIncludedTerm);
    snapshotData = sd;
    persist();
}

template <typename Client>
void RaftImpl<Client>::persist() {
    persister.save(PersistentState {
        currentTerm,
        votedFor,
        Log {mainLog.data()},
        snapshotData,
        lastIncludedIndex,
        lastIncludedTerm
    });
}

template <typename Client>
void RaftImpl<Client>::readPersist(PersistentState s) {
    std::unique_lock lock{m};
    if (killed.load()) {
        return;
    }
    if (pendingSnapshot.load()) {
        spdlog::warn("{}: readPersist: another snapshot is pending, skipping", selfId);
        return;
    }
    spdlog::info("{}: readPersist: currentTerm={}, votedFor={}, logSize={} lastIncludedIndex={}, lastIncludedTerm={}", selfId, s.currentTerm, s.votedFor.has_value() ? s.votedFor.value() : "null", s.log.data().size(), s.lastIncludedIndex, s.lastIncludedTerm);
    currentTerm = s.currentTerm;
    votedFor = s.votedFor;
    mainLog.clear();
    mainLog.merge(s.log);
    mainLog.setLastIncluded(s.lastIncludedIndex, s.lastIncludedTerm);
    auto uuid = generate_uuid_v7();
    lastApplied = std::max(lastApplied, s.lastIncludedIndex);
    commitIndex = std::max(commitIndex, s.lastIncludedIndex);
    lastIncludedIndex = s.lastIncludedIndex;
    lastIncludedTerm = s.lastIncludedTerm;
    snapshotData = s.snapshotData;
    if (s.snapshotData.empty()) {
        return;
    }
    lock.unlock();
    if (!stateMachine.sendUntil(std::make_shared<zdb::InstallSnapshotCommand>(uuid, s.lastIncludedIndex, s.lastIncludedTerm, s.snapshotData), std::chrono::system_clock::now() + policy.rpcTimeout)) {
        spdlog::error("{}: readPersist: failed to apply snapshot", selfId);
        pendingSnapshot.store(true);
    }
}

template <typename Client>
void RaftImpl<Client>::kill(){
    killed.store(true);
    stopCalls.store(true);
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
