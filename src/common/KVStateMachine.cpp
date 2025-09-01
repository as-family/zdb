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
      t {std::thread(&KVStateMachine::consumeChannel, this)} {}

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

State KVStateMachine::handleGet(Key key, std::chrono::system_clock::time_point t) {
    auto c = Get{key};
    raft.start(c.serialize());
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{key, std::expected<std::optional<Value>, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto value = storageEngine.get(key);
    return State{key, value};
}

State KVStateMachine::handleSet(Key key, Value value, std::chrono::system_clock::time_point t) {
    auto c = Set{key, value};
    raft.start(c.serialize());
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{key, std::expected<std::monostate, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto result = storageEngine.set(key, value);
    return State{key, result};
}

State KVStateMachine::handleErase(Key key, std::chrono::system_clock::time_point t) {
    auto c = Erase{key};
    raft.start(c.serialize());
    auto r = leader.receiveUntil(t);
    if (!r.has_value()) {
        return State{key, std::expected<std::optional<Value>, Error>{std::unexpected(Error{ErrorCode::Timeout, "request timed out"})}};
    }
    auto result = storageEngine.erase(key);
    return State{key, result};
}

State KVStateMachine::handleSize(std::chrono::system_clock::time_point t) {
    auto c = Size{};
    raft.start(c.serialize());
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
    if (t.joinable()) {
        t.join();
    }
}

} // namespace zdb
