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

namespace raft {

RaftImpl::RaftImpl(std::vector<std::string> p, std::string s, Channel& c, zdb::RetryPolicy r, Command* (*f)(const std::string&))
    : serviceChannel {c},
      policy {r},
      fullJitter {},
      electionTimer {},
      heartbeatTimer {},
      lastHeartbeat {std::chrono::steady_clock::now() - std::chrono::days(100)},
      commandFactory {f},
      mainLog {f} {
    selfId = s;
    clusterSize = p.size();
    nextIndex = std::unordered_map<std::string, uint64_t>{};
    matchIndex = std::unordered_map<std::string, uint64_t>{};
    for (const auto& a : p) {
        nextIndex[a] = 1;
        matchIndex[a] = 0;
    }
    p.erase(std::find(p.begin(), p.end(), selfId));
    for (const auto& peer : p) {
        peers.emplace(std::piecewise_construct,
                       std::forward_as_tuple(peer),
                       std::forward_as_tuple(peer, policy));
    }
    electionTimeout = std::chrono::milliseconds(100);
    heartbeatInterval = std::chrono::milliseconds(20);

    electionTimer.start(
        [this] -> std::chrono::milliseconds {
            return electionTimeout +
                   std::chrono::duration_cast<std::chrono::milliseconds>(fullJitter.jitter(
                    std::chrono::duration_cast<std::chrono::microseconds>(electionTimeout)));
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
            if (mainLog.lastTerm() == arg.lastLogTerm && mainLog.lastIndex() < arg.lastLogIndex || mainLog.lastTerm() < arg.lastLogTerm) {
                std::unique_lock voteLock {m};
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
        }
    }
    reply.term = currentTerm;
    reply.success = false;
    return reply;
}

void RaftImpl::appendEntries(){
    auto g = mainLog.suffix(mainLog.lastIndex());
    AppendEntriesArg arg {
        selfId,
        currentTerm,
        0,
        0,
        commitIndex,
        g
    };

}
void RaftImpl::requestVote(){

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
