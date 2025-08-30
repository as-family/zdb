#include "common/KVStateMachine.hpp"
#include "raft/StateMachine.hpp"
#include "raft/Command.hpp"
#include "common/Command.hpp"

namespace zdb {

KVStateMachine::KVStateMachine(StorageEngine* s, raft::Channel* leaderChannel, raft::Channel* followerChannel, raft::Raft* r)
    : storageEngine(s),
      leader(leaderChannel),
      follower(followerChannel),
      raft {r},
      t {std::thread(&KVStateMachine::consumeChannel, this)} {}

raft::State* KVStateMachine::applyCommand(raft::Command* command) {
    return command->apply(this);
}

void KVStateMachine::consumeChannel() {
    while (true) {
        std::string s = follower->receive();
        if (s == "") {
            break;
        }
        auto command = commandFactory(s);
        command->apply(this);
    }
}

void KVStateMachine::snapshot() {
    // Create a snapshot of the current state
}

void KVStateMachine::restore(const std::string& snapshot) {
    // Restore the state from a snapshot
}

raft::State* KVStateMachine::get(Key key) {
    auto value = storageEngine->get(key);
    return new State{key, value};
}

raft::State* KVStateMachine::set(Key key, Value value) {
    auto result = storageEngine->set(key, value);
    return new State{key, result};
}

raft::State* KVStateMachine::erase(Key key) {
    auto result = storageEngine->erase(key);
    return new State{key, result};
}

raft::State* KVStateMachine::size() {
    auto result = storageEngine->size();
    return new State{result};
}

raft::State* KVStateMachine::handleGet(Key key) {
    auto c = Get{key};
    raft->start(c.serialize());
    leader->receive();
    auto value = storageEngine->get(key);
    return new State{key, value};
}

raft::State* KVStateMachine::handleSet(Key key, Value value) {
    auto c = Set{key, value};
    raft->start(c.serialize());
    leader->receive();
    auto result = storageEngine->set(key, value);
    return new State{key, result};
}

raft::State* KVStateMachine::handleErase(Key key) {
    auto c = Erase{key};
    raft->start(c.serialize());
    leader->receive();
    auto result = storageEngine->erase(key);
    return new State{key, result};
}

raft::State* KVStateMachine::handleSize() {
    auto c = Size{};
    raft->start(c.serialize());
    leader->receive();
    auto size = storageEngine->size();
    return new State{size};
}

KVStateMachine::~KVStateMachine() {
    follower->send("");
    if (t.joinable()) {
        t.join();
    }
}

} // namespace zdb

