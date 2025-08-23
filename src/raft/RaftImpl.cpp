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

namespace raft {

RaftImpl::RaftImpl(std::vector<std::string> p, std::string s, Channel& c)
    : serviceChannel(c),
      policy{std::chrono::milliseconds(10), std::chrono::milliseconds(100), std::chrono::milliseconds(1000), 3, 1},
      raftService {this},
      server {s, raftService},
      fullJitter{},
      electionTimer{},
      killed {false} {
    selfId = s;
    peerAddresses = p;
    peerAddresses.erase(std::find(peerAddresses.begin(), peerAddresses.end(), selfId));
    for (const auto& peer : peerAddresses) {
        peers.emplace(std::piecewise_construct,
                       std::forward_as_tuple(peer),
                       std::forward_as_tuple(peer, policy));
    }
    electionTimer.start(
        [this] -> std::chrono::milliseconds {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                electionTimeout + fullJitter.jitter(electionTimeout)
            );
        },
        [this] {
            requestVote();
        }
    );
}

void RaftImpl::requestVote() {
    if (role == Role::Leader) return;
    if (role == Role::Follower) {
        role = Role::Candidate;
        currentTerm += 1;
        votedFor = selfId;
        votesGranted = 1;
        downPeers = 0;
        votesDeclined = 0;
        electionEnded = false;
        for (auto& peer : peers) {
            auto vote = [this, &peer]() {
                proto::RequestVoteArg arg;
                arg.set_term(currentTerm);
                arg.set_candidateid(selfId);
                arg.set_lastlogindex(log.lastIndex());
                arg.set_lastlogterm(log.lastTerm());
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
                            if (votesGranted >= peers.size() / 2 + 1 && !electionEnded) {
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
                            if (votesDeclined + downPeers > peers.size() / 2) {
                                electionEnded = true;
                                votedFor.reset();
                                electionCondVar.notify_all();
                            }
                        }
                    } else {
                        if (!zdb::isRetriable("requestVote", status.error().back().code)) {
                            std::lock_guard<std::mutex> lock(electionMutex);
                            ++downPeers;
                            if (downPeers + votesDeclined > peers.size() / 2) {
                                electionEnded = true;
                                votedFor.reset();
                                electionCondVar.notify_all();
                            }
                        }
                    }
                }
            };
            std::thread t {vote};
        }
        while(!electionEnded) {
            std::unique_lock<std::mutex> lock(electionMutex);
            electionCondVar.wait(lock, [this] { return electionEnded; });
        }
        appendEntries();
    }
}

void RaftImpl::appendEntries() {
    if (role != Role::Leader) throw std::runtime_error("Not the leader");
    for (auto& peer : peers) {
        auto sendEntries = [this, &peer]() {
            proto::AppendEntriesArg arg;
            arg.set_leaderid(selfId);
            arg.set_term(currentTerm);
            arg.set_prevlogindex(log.lastIndex());
            arg.set_prevlogterm(log.lastTerm());
            arg.set_leadercommit(commitIndex);
            for (auto i = log.entries.begin() + commitIndex; i != log.entries.end(); ++i) {
                auto entry = *i;
                auto e = arg.add_entries();
                e->set_index(entry.index);
                e->set_term(entry.term);
                e->set_command(entry.command->serialize());
            }
            while (role == Role::Leader && !killed) {
                proto::AppendEntriesReply reply;
                auto status = peer.second.call(
                    "appendEntries",
                    &proto::Raft::Stub::appendEntries,
                    arg,
                    reply
                );
                if (status.has_value()) {
                    if (reply.success()) {
                        break;
                    } else {
                        if (reply.term() > currentTerm) {
                            std::lock_guard<std::mutex> lock(electionMutex);
                            currentTerm = reply.term();
                            role = Role::Follower;
                            votedFor.reset();
                        }
                    }
                } else {
                    if (!zdb::isRetriable("appendEntries", status.error().back().code)) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        };
        std::thread t {sendEntries};
    }
}

AppendEntriesReply RaftImpl::appendEntriesHandler(const AppendEntriesArg& arg) {
    AppendEntriesReply reply;
    if(arg.term < currentTerm) {
        reply.success = false;
        reply.term = currentTerm;
        return reply;
    }
    if (log.entries.size() > arg.prevLogIndex && log.entries[arg.prevLogIndex].term != arg.prevLogTerm) {
        reply.success = false;
        reply.term = currentTerm;
        return reply;
    }
    for (const auto& entry : arg.entries) {
        if (entry.index <= log.lastIndex()) {
            if (log.entries[entry.index].term != entry.term) {
                log.entries[entry.index] = entry;
            }
        } else {
            log.entries.push_back(entry);
        }
    }
    if (arg.leaderCommit > commitIndex) {
        commitIndex = std::min(arg.leaderCommit, log.lastIndex());
    }
    reply.success = true;
    reply.term = currentTerm;
    return reply;
}

RequestVoteReply RaftImpl::requestVoteHandler(const RequestVoteArg& arg) {
    RequestVoteReply reply;
    reply.term = currentTerm;
    if (arg.term < currentTerm ||
        votedFor.has_value() && votedFor.value() != arg.candidateId) {
        reply.voteGranted = false;
    } else if (arg.lastLogIndex >= log.lastIndex() &&
               arg.lastLogTerm >= log.lastTerm()) {
        votedFor = arg.candidateId;
        reply.voteGranted = true;
    }
    return reply;
}

void RaftImpl::start(Command* command) {
}

void RaftImpl::kill() {
    server.shutdown();
    electionTimer.stop();
    killed = true;
}

RaftImpl::~RaftImpl() {
}

} // namespace raft
