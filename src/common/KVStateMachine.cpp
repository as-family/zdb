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

void KVStateMachine::applyCommand(raft::Command& command) {
    command.apply(*this);
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
    auto value = storageEngine.get(key);
    return State{key, value};
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

State KVStateMachine::handleGet(Get c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {
        return State{c.key, std::expected<std::optional<Value>, Error>{
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}};
    }
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{c.key, std::expected<std::optional<Value>, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto value = storageEngine.get(c.key);
    return State{c.key, value};
}

State KVStateMachine::handleSet(Set c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {  
        return State{c.key, std::expected<std::optional<Value>, Error>{  
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}};  
    } 
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{c.key, std::expected<std::monostate, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto result = storageEngine.set(c.key, c.value);
    return State{c.key, result};
}

State KVStateMachine::handleErase(Erase c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {  
        return State{c.key, std::expected<std::optional<Value>, Error>{  
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}};  
    } 
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{c.key, std::expected<std::optional<Value>, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto result = storageEngine.erase(c.key);
    return State{c.key, result};
}

State KVStateMachine::handleSize(Size c, std::chrono::system_clock::time_point t) {
    if (!raft.start(c.serialize())) {  
        return State{std::expected<size_t, Error>{  
            std::unexpected(Error{ErrorCode::NotLeader, "not the leader"})}};  
    } 
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{std::expected<size_t, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto size = storageEngine.size();
    return State{size};
}

KVStateMachine::~KVStateMachine() {
    follower.close();
    leader.close();
    if (consumerThread.joinable()) {
        consumerThread.join();
    }
}

} // namespace zdb
