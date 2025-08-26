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
      policy{std::chrono::milliseconds(5), std::chrono::milliseconds(50), std::chrono::days(1000), 10, 1},
      fullJitter{},
      electionTimer{},
      heartbeatTimer{},
      lastHeartbeat{std::chrono::steady_clock::now() - std::chrono::seconds(1000)},
      threads {},
      commandFactory {f},
      mainLog {f},
      killed {false} {
    selfId = s;
    electionEnded = true;
    electionTimeout = std::chrono::milliseconds(100);
    heartbeatInterval = std::chrono::milliseconds(20);
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
                electionTimeout + fullJitter.jitter(electionTimeout / 3)
            );
        },
        [this] {
            requestVote();
        }
    );
    heartbeatTimer.start(
        [this] -> std::chrono::milliseconds {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                heartbeatInterval + fullJitter.jitter(heartbeatInterval / 3)
            );
        },
        [this] {
            std::unique_lock lg{globalLock};
            appendEntries();
        }
    );
}

void RaftImpl::requestVote() {
    std::unique_lock lg{globalLock};
    if (std::chrono::steady_clock::now() - lastHeartbeat < electionTimeout) {
        return;
    }
    if (role == Role::Leader) {
        return;
    }
    if (!electionEnded) {
        return;
    }
    {
        // std::unique_lock<std::mutex> lock(electionMutex);
        role = Role::Candidate;
        electionEnded = false;
        ++currentTerm;
        votedFor = selfId;
        votesGranted = 1;
        downPeers = 0;
        votesDeclined = 0;
    }
    for (auto& peer : peers) {
        auto vote = [this, &peer]() {
            proto::RequestVoteArg arg;
            arg.set_term(currentTerm);
            arg.set_candidateid(selfId);
            arg.set_lastlogindex(mainLog.lastIndex());
            arg.set_lastlogterm(mainLog.lastTerm());
            while(!electionEnded && !killed) {
                if (downPeers + votesDeclined > clusterSize / 2) {
                    electionEnded = true;
                    electionCondVar.notify_all();
                    break;
                }
                if (downPeers + votesGranted + votesDeclined >= clusterSize) {
                    electionEnded = true;
                    electionCondVar.notify_all();
                    break;
                }
                proto::RequestVoteReply reply;
                auto status = peer.second.call(
                    "requestVote",
                    &proto::Raft::Stub::requestVote,
                    arg,
                    reply
                );
                // std::cerr << "Vote: " << selfId << " from " << peer.first << " for term " << currentTerm;
                if (status.has_value()) {
                    // std::cerr << ": " << (reply.votegranted() ? "granted" : "not granted") << std::endl;
                    if (reply.votegranted()) {
                        //std::unique_lock<std::mutex> lock(electionMutex);
                        ++votesGranted;
                        // std::cerr << selfId << " got vote from " << peer.first << " for term " << currentTerm << std::endl;
                        if (votesGranted >= clusterSize / 2 + 1 && !electionEnded) {
                            role = Role::Leader;
                            electionEnded = true;
                            electionCondVar.notify_all();
                            break;
                        }
                    } else {
                        // std::unique_lock<std::mutex> lock(electionMutex);
                        ++votesDeclined;
                        if (reply.term() > currentTerm) {
                            currentTerm = reply.term();
                            role = Role::Follower;
                            lastHeartbeat = std::chrono::steady_clock::now();
                            electionEnded = true;
                            votedFor.reset();
                            electionCondVar.notify_all();
                            break;
                        }
                        if (votesDeclined + downPeers > clusterSize / 2) {
                            electionEnded = true;
                            electionCondVar.notify_all();
                            break;
                        }
                    }
                } else {
                    // std::cerr << ": " << status.error().back().what << std::endl;
                    if (!zdb::isRetriable("requestVote", status.error().back().code)) {
                        // std::unique_lock<std::mutex> lock(electionMutex);
                        ++downPeers;
                        break;
                    }
                }
            }
        };
        std::thread t {vote};
        threads.push_back(std::move(t));
    }
    while(!electionEnded && !killed && role == Role::Candidate && downPeers + votesDeclined <= clusterSize / 2 && downPeers + votesGranted + votesDeclined < clusterSize) {
        // std::unique_lock<std::mutex> lock(electionMutex);
        // electionCondVar.wait_for(lg, std::chrono::milliseconds(5), [this] { return electionEnded || role != Role::Candidate || killed; });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (killed) {
        return;
    }
    if (role == Role::Leader) {
        // std::cerr << selfId << " became leader" << std::endl;
        appendEntries();
    }
}

void RaftImpl::appendEntries() {
    {
        // std::unique_lock<std::mutex> e(electionMutex);
        if(role != Role::Leader) {
            return;
        }
    }
    {
        // std::unique_lock<std::mutex> lock(appendEntriesMutex);
        nReplies = 1;
        nDown = 0;
    }
    for (auto& peerAddress : peerAddresses) {
        auto sendEntries = [this, &peerAddress]() {
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
            while (role == Role::Leader && !killed && nDown + nReplies < clusterSize) {
                proto::AppendEntriesReply reply;
                auto status = peer.call(
                    "appendEntries",
                    &proto::Raft::Stub::appendEntries,
                    arg,
                    reply
                );
                // std::cerr << "Append: " << selfId << " to " << peerAddress << " for term " << currentTerm;
                if (status.has_value()) {
                    // std::cerr << " got reply: " << reply.success() << std::endl;
                    if (reply.success()) {
                        // std::unique_lock<std::mutex> lock(appendEntriesMutex);
                        ++nReplies;
                        nextIndex[peerAddress] = mainLog.lastIndex() + 1;
                        matchIndex[peerAddress] = mainLog.lastIndex();
                        if (nReplies >= clusterSize / 2 + 1) {
                            appendEntriesCondVar.notify_all();
                        }
                        break;
                    } else {
                        if (reply.term() > currentTerm) {
                            // std::unique_lock<std::mutex> lock2(appendEntriesMutex);
                            // std::unique_lock<std::mutex> lock(electionMutex);
                            currentTerm = reply.term();
                            role = Role::Follower;
                            votedFor.reset();
                            electionEnded = true;
                            lastHeartbeat = std::chrono::steady_clock::now();
                            electionCondVar.notify_all();
                            appendEntriesCondVar.notify_all();
                            break;
                        } else {
                            // std::cerr << "Append: " << selfId << " to " << peerAddress << " for term " << currentTerm << " got reply: Not up to date" << std::endl;
                            nextIndex[peerAddress] = std::max<int64_t>(1, nextIndex[peerAddress] - 1);
                        }
                    }
                } else {
                    std::cerr << " got error: " << status.error().back().what << std::endl;
                    if (!zdb::isRetriable("appendEntries", status.error().back().code)) {
                        ++nDown;
                        break;
                    }
                }
            }
        };
        std::thread t {sendEntries};
        threads.push_back(std::move(t));
    }
    while(nReplies < clusterSize / 2 + 1 && !killed && role == Role::Leader && nReplies + nDown <= clusterSize) {
        // std::unique_lock<std::mutex> lock(appendEntriesMutex);
        // appendEntriesCondVar.wait_for(lock, std::chrono::milliseconds(5), [&] { return nReplies >= clusterSize / 2 + 1 || role != Role::Leader || killed; });
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // std::cerr << selfId << " got " << nReplies << " replies out of " << static_cast<int>(clusterSize) << std::endl;
    if (killed) {
        return;
    }
    if (role != Role::Leader) {
        return;
    }
    if (nReplies < clusterSize / 2 + 1) {
        return;
    }
    commitIndex = mainLog.lastIndex();
    auto l = mainLog.suffix(lastApplied);
    for (const auto& entry : l) {
        serviceChannel.send(entry.command);
    }
    lastApplied = l.empty() ? 0 : l.back().index;
}

AppendEntriesReply RaftImpl::appendEntriesHandler(const AppendEntriesArg& arg) {
    std::unique_lock<std::mutex> gl{globalLock};
    AppendEntriesReply reply;
    if(arg.term < currentTerm) {
        reply.success = false;
        reply.term = currentTerm;
        return reply;
    }
    if (mainLog.at(arg.prevLogIndex).has_value() && mainLog.at(arg.prevLogIndex).value().term != arg.prevLogTerm) {
        reply.success = false;
        reply.term = currentTerm;
        return reply;
    }
    lastHeartbeat = std::chrono::steady_clock::now();
    {
        //std::unique_lock<std::mutex> lock(electionMutex);
        electionEnded = true;
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
    std::unique_lock g{globalLock};
    RequestVoteReply reply;
    if (currentTerm > arg.term) {
        reply.voteGranted = false;
        reply.term = currentTerm;
        return reply;
    }
    // std::unique_lock<std::mutex> lock(electionMutex);
    if (votedFor.has_value() && votedFor.value() != arg.candidateId) {
        reply.voteGranted = false;
        reply.term = currentTerm;
        return reply;
    }
    if (votedFor.has_value() && votedFor.value() == arg.candidateId) {
        lastHeartbeat = std::chrono::steady_clock::now();
        votedFor = arg.candidateId;
        reply.voteGranted = true;
        currentTerm = arg.term;
        reply.term = currentTerm;
        role = Role::Follower;
        electionEnded = true;
        electionCondVar.notify_all();
        return reply;
    }
    if(!votedFor.has_value()) {
        if (arg.lastLogTerm > mainLog.lastTerm() || (arg.lastLogTerm == mainLog.lastTerm() && arg.lastLogIndex >= mainLog.lastIndex())) {
            lastHeartbeat = std::chrono::steady_clock::now();
            votedFor = arg.candidateId;
            reply.voteGranted = true;
            currentTerm = arg.term;
            reply.term = currentTerm;
            role = Role::Follower;
            electionEnded = true;
            electionCondVar.notify_all();
            return reply;
        } else {
            reply.voteGranted = false;
            reply.term = currentTerm;
            return reply;
        }
    }
    throw std::runtime_error("Should not reach here in requestVoteHandler");
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

void RaftImpl::connectPeers() {
    for (auto& peer : peers) {
        if (!peer.second.available()) {
            throw std::runtime_error(selfId + " Could not connect to peer " + peer.first);
        }
    }
}

void RaftImpl::kill() {
    std::unique_lock<std::mutex> lock(globalLock);
    // std::unique_lock<std::mutex> lock2(electionMutex);
    killed = true;
    // lock2.unlock();
    // lock.unlock();
    electionTimer.stop();
    heartbeatTimer.stop();
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

RaftImpl::~RaftImpl() {
    if (!killed) {
        kill();
    }
}

} // namespace raft
