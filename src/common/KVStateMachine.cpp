#include "common/KVStateMachine.hpp"
#include "raft/StateMachine.hpp"
#include "raft/Command.hpp"
#include "common/Command.hpp"

namespace zdb {

KVStateMachine::KVStateMachine(StorageEngine& s, raft::Channel& leaderChannel, raft::Channel& followerChannel, raft::Raft& r)
    : storageEngine(s),
      leader(leaderChannel),
      follower(followerChannel),
      raft {r},
      consumerThread {std::thread(&KVStateMachine::consumeChannel, this)} {}

std::unique_ptr<raft::State> KVStateMachine::applyCommand(raft::Command& command) {
    return command.apply(*this);
}

void KVStateMachine::consumeChannel() {
    
    while (!follower.isClosed()) {
        std::string s = follower.receive();
        if (s == "") {
            break;
        }
        auto command = commandFactory(s);
        command->apply(*this);
    }
}

void KVStateMachine::snapshot() {
    // Create a snapshot of the current state
}

void KVStateMachine::restore(const std::string& snapshot) {
    // Restore the state from a snapshot
    std::ignore = snapshot;
}

State KVStateMachine::get(Key key) {
    auto result = storageEngine.get(key);
    return State{key, result};
}

State KVStateMachine::set(Key key, Value value) {
    auto result = storageEngine.set(key, value);
    return State{key, result};
}

State KVStateMachine::erase(Key key) {
    auto result = storageEngine.erase(key);
    return State{key, result};
}

State KVStateMachine::size() {
    auto result = storageEngine.size();
    return State{result};
}

std::unique_ptr<raft::State> KVStateMachine::handleGet(Get c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {
        return std::make_unique<State>(State{c.key, std::expected<std::optional<Value>, Error>{
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}});
    }
    while (true) {
        auto r = leader.receiveUntil(t);
        if (!r.has_value()) {
            return std::make_unique<State>(State{c.key, std::expected<std::optional<Value>, Error>{
                std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}});
        }
        auto cc = commandFactory(r.value());
        auto s = applyCommand(*cc);
        if (cc->getUUID() == c.getUUID()) {
            return s;
        }
    }
}

std::unique_ptr<raft::State> KVStateMachine::handleSet(Set c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {
        return std::make_unique<State>(State{c.key, std::expected<std::optional<Value>, Error>{
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}});
    }
    while (true) {
        auto r = leader.receiveUntil(t);
        if (!r.has_value()) {
            return std::make_unique<State>(State{c.key, std::expected<std::monostate, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}});
        }
        auto cc = commandFactory(r.value());
        auto s = applyCommand(*cc);
        if(cc->getUUID() == c.getUUID()) {
            return s;
        }
    }
}

std::unique_ptr<raft::State> KVStateMachine::handleErase(Erase c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {  
        return std::make_unique<State>(State{c.key, std::expected<std::optional<Value>, Error>{  
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}});  
    }
    while (true) {
        auto r = leader.receiveUntil(t);
        if (!r.has_value()) {
            return std::make_unique<State>(State{c.key, std::expected<std::monostate, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}});
        }
        auto cc = commandFactory(r.value());
        auto s = applyCommand(*cc);
        if(cc->getUUID() == c.getUUID()) {
            return s;
        }
    }
}

std::unique_ptr<raft::State> KVStateMachine::handleSize(Size c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {  
        return std::make_unique<State>(State{std::expected<size_t, Error>{  
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}});  
    }
    while (true) {
        auto r = leader.receiveUntil(t);
        if (!r.has_value()) {
            return std::make_unique<State>(State{std::expected<size_t, Error>{
                std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}});
        }
        auto cc = commandFactory(r.value());
        auto s = applyCommand(*cc);
        if(cc->getUUID() == c.getUUID()) {
            return s;
        }
    }
}

KVStateMachine::~KVStateMachine() {
    follower.close();
    leader.close();
    if (consumerThread.joinable()) {
        consumerThread.join();
    }
}

} // namespace zdb
