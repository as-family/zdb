#include "raft/RaftImpl.hpp"
#include "raft/Types.hpp"
#include <tuple>
#include <algorithm>
#include "raft/RaftServiceImpl.hpp"
#include "raft/AsyncTimer.hpp"
#include <mutex>
#include <condition_variable>
#include "common/Error.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace raft {

RaftImpl::RaftImpl(std::vector<std::string> p, std::string s, Channel& c, Command* (*f)(const std::string&))
    : serviceChannel(c),
      policy{std::chrono::milliseconds(10), std::chrono::milliseconds(100), std::chrono::milliseconds(1000), 3, 1},
      raftService {this},
      server {s, raftService},
      fullJitter{},
      electionTimer{},
      heartbeatTimer{},
      lastHeartbeat{std::chrono::steady_clock::now() - std::chrono::seconds(1000)},
      threads {},
      commandFactory {f},
      mainLog {f},
      killed {false} {
    selfId = s;
    electionTimeout = std::chrono::milliseconds(40);
    heartbeatInterval = std::chrono::milliseconds(15);
    peerAddresses = std::vector<std::string>(p);
    clusterSize = static_cast<uint8_t>(peerAddresses.size());
    matchIndex = std::unordered_map<std::string, uint64_t>();
    nextIndex = std::unordered_map<std::string, uint64_t>();
    peerAddresses.erase(std::find(peerAddresses.begin(), peerAddresses.end(), selfId));
    for (const auto& peer : peerAddresses) {
        peers.emplace(std::piecewise_construct,
                       std::forward_as_tuple(peer),
                       std::forward_as_tuple(peer, policy));
        matchIndex[peer] = 0;
        nextIndex[peer] = 1;
    }
    electionTimer.start(
        [this] -> std::chrono::milliseconds {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                electionTimeout + fullJitter.jitter(electionTimeout / 5)
            );
        },
        [this] {
            if (std::chrono::steady_clock::now() - lastHeartbeat > electionTimeout) {
                requestVote();
            }
        }
    );
    heartbeatTimer.start(
        [this] -> std::chrono::milliseconds {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                heartbeatInterval + fullJitter.jitter(heartbeatInterval / 5)
            );
        },
        [this] {
            if (role == Role::Leader) {
                appendEntries();
            }
        }
    );
}

void RaftImpl::requestVote() {
    if (role == Role::Leader) return;
    if (role == Role::Candidate && !electionEnded) return;
    if (role == Role::Follower) {
        role = Role::Candidate;
        electionEnded = false;
        ++currentTerm;
        votedFor = selfId;
        votesGranted = 1;
        downPeers = 0;
        votesDeclined = 0;
        for (auto& peer : peers) {
            auto vote = [this, &peer]() {
                proto::RequestVoteArg arg;
                arg.set_term(currentTerm);
                arg.set_candidateid(selfId);
                arg.set_lastlogindex(mainLog.lastIndex());
                arg.set_lastlogterm(mainLog.lastTerm());
                while(!electionEnded && !killed) {
                    proto::RequestVoteReply reply;
                    auto status = peer.second.call(
                        "requestVote",
                        &proto::Raft::Stub::requestVote,
                        arg,
                        reply
                    );
                    if (status.has_value()) {
                        if (reply.votegranted()) {
                            std::lock_guard<std::mutex> lock(electionMutex);
                            ++votesGranted;
                            if (votesGranted >= clusterSize / 2 + 1 && !electionEnded) {
                                electionEnded = true;
                                role = Role::Leader;
                                electionCondVar.notify_all();
                            }
                        } else {
                            std::lock_guard<std::mutex> lock(electionMutex);
                            if (reply.term() > currentTerm) {
                                currentTerm = reply.term();
                                role = Role::Follower;
                                votedFor.reset();
                                electionEnded = true;
                                electionCondVar.notify_all();
                            }
                            ++votesDeclined;
                            if (votesDeclined + downPeers > clusterSize / 2) {
                                electionEnded = true;
                                votedFor.reset();
                                electionCondVar.notify_all();
                            }
                        }
                    } else {
                        if (!zdb::isRetriable("requestVote", status.error().back().code)) {
                            std::lock_guard<std::mutex> lock(electionMutex);
                            ++downPeers;
                            if (downPeers + votesDeclined > clusterSize / 2) {
                                electionEnded = true;
                                votedFor.reset();
                                electionCondVar.notify_all();
                            }
                        }
                    }
                }
            };
            std::thread t {vote};
            threads.push_back(std::move(t));
        }
        while(!electionEnded && !killed) {
            std::unique_lock<std::mutex> lock(electionMutex);
            electionCondVar.wait_for(lock, std::chrono::milliseconds(25), [this] { return electionEnded; });
        }
        if (killed) {
            return;
        }
        if (role == Role::Leader) {
            appendEntries();
        }
    }
}

void RaftImpl::appendEntries() {
    if (role != Role::Leader) throw std::runtime_error("Not the leader");
    std::mutex m{};
    std::condition_variable cv{};
    std::atomic<int> nReplies {0};
    for (auto& peerAddress : peerAddresses) {
        auto sendEntries = [this, &peerAddress, &nReplies, &m, &cv]() {
            auto& peer = peers.at(peerAddress);
            proto::AppendEntriesArg arg;
            arg.set_leaderid(selfId);
            arg.set_term(currentTerm);
            arg.set_prevlogindex(mainLog.lastIndex());
            arg.set_prevlogterm(mainLog.lastTerm());
            arg.set_leadercommit(commitIndex);
            auto l = mainLog.suffix(nextIndex[peerAddress]);
            for (auto i = l.begin(); i != l.end(); ++i) {
                auto entry = *i;
                auto e = arg.add_entries();
                e->set_index(entry.index);
                e->set_term(entry.term);
                e->set_command(entry.command->serialize());
            }
            while (role == Role::Leader && !killed) {
                proto::AppendEntriesReply reply;
                auto status = peer.call(
                    "appendEntries",
                    &proto::Raft::Stub::appendEntries,
                    arg,
                    reply
                );
                if (status.has_value()) {
                    if (reply.success()) {
                        std::unique_lock<std::mutex> lock(m);
                        ++nReplies;
                        nextIndex[peerAddress] = mainLog.lastIndex() + 1;
                        matchIndex[peerAddress] = mainLog.lastIndex();
                        cv.notify_all();
                        break;
                    } else {
                        if (reply.term() > currentTerm) {
                            std::lock_guard<std::mutex> lock(electionMutex);
                            currentTerm = reply.term();
                            role = Role::Follower;
                            votedFor.reset();
                            electionEnded = true;
                            electionCondVar.notify_all();
                            break;
                        } else {
                            nextIndex[peerAddress] = std::max<int64_t>(1, nextIndex[peerAddress] - 1);
                        }
                    }
                } else {
                    if (!zdb::isRetriable("appendEntries", status.error().back().code)) {
                        break;
                    }
                }
            }
        };
        std::thread t {sendEntries};
        threads.push_back(std::move(t));
    }
    while(nReplies < clusterSize / 2 + 1 && !killed) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait_for(lock, std::chrono::milliseconds(10), [&] { return nReplies >= clusterSize / 2 + 1; });
    }
    if (killed) {
        return;
    }
    commitIndex = mainLog.lastIndex();
    auto l = mainLog.suffix(lastApplied);
    for (const auto& entry : l) {
        serviceChannel.send(entry.command);
    }
    lastApplied = l.back().index;
}

AppendEntriesReply RaftImpl::appendEntriesHandler(const AppendEntriesArg& arg) {
    lastHeartbeat = std::chrono::steady_clock::now();
    AppendEntriesReply reply;
    if(arg.term < currentTerm) {
        reply.success = false;
        reply.term = currentTerm;
        return reply;
    }
    if (mainLog.lastIndex() > arg.prevLogIndex && mainLog.lastTerm() != arg.prevLogTerm) {
        reply.success = false;
        reply.term = currentTerm;
        return reply;
    }
    {
        std::lock_guard<std::mutex> lock(electionMutex);
        electionEnded = true;
        votedFor.reset();
        role = Role::Follower;
        electionCondVar.notify_all();
    }
    mainLog.merge(arg.entries);
    if (arg.leaderCommit > commitIndex) {
        commitIndex = std::min(arg.leaderCommit, mainLog.lastIndex());
    }
    reply.success = true;
    reply.term = currentTerm;
    return reply;
}

RequestVoteReply RaftImpl::requestVoteHandler(const RequestVoteArg& arg) {
    lastHeartbeat = std::chrono::steady_clock::now();
    RequestVoteReply reply;
    if (currentTerm > arg.term) {
        reply.voteGranted = false;
        reply.term = currentTerm;
        return reply;
    }
    if (arg.term > currentTerm) {
        std::lock_guard<std::mutex> lock(electionMutex);
        currentTerm = arg.term;
        role = Role::Follower;
        votedFor.reset();
        lastHeartbeat = std::chrono::steady_clock::now();
        electionEnded = true;
        electionCondVar.notify_all();
    }
    if (votedFor.has_value() && votedFor.value() != arg.candidateId) {
        reply.voteGranted = false;
        reply.term = currentTerm;
        return reply;
    }
    if (votedFor.has_value() && votedFor.value() == arg.candidateId) {
        reply.voteGranted = true;
        reply.term = currentTerm;
        lastHeartbeat = std::chrono::steady_clock::now();
        return reply;
    }
    if(!votedFor.has_value()) {
        if (arg.lastLogTerm < mainLog.lastTerm() || (arg.lastLogTerm == mainLog.lastTerm() && arg.lastLogIndex < mainLog.lastIndex())) {
            reply.voteGranted = false;
            reply.term = currentTerm;
            return reply;
        } else {
            std::lock_guard<std::mutex> lock(electionMutex);
            votedFor = arg.candidateId;
            reply.voteGranted = true;
            reply.term = currentTerm;
            role = Role::Follower;
            electionEnded = true;
            lastHeartbeat = std::chrono::steady_clock::now();
            electionCondVar.notify_all();
            return reply;
        }
    }
    std::unreachable();
}

bool RaftImpl::start(Command* command) {
    if (role != Role::Leader) {
        return false;
    }
    auto e = LogEntry {
        mainLog.lastIndex() + 1,
        currentTerm,
        command
    };
    mainLog.append(e);
    return true;
}

Log& RaftImpl::log() {
    return mainLog;
}

Log* RaftImpl::makeLog() {
    return new Log(commandFactory);
}

void RaftImpl::kill() {
    killed = true;
    server.shutdown();
    electionTimer.stop();
    heartbeatTimer.stop();
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    role = Role::Follower;
}

RaftImpl::~RaftImpl() {
    if (!killed) {
        kill();
    }
}

} // namespace raft
