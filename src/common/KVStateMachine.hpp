#ifndef KV_STATEMACHINE_H
#define KV_STATEMACHINE_H

#include "raft/StateMachine.hpp"
#include "common/Types.hpp"
#include "raft/Channel.hpp"
#include <thread>
#include "interface/StorageEngine.hpp"
#include "raft/Raft.hpp"
#include <chrono>

namespace zdb {

class KVStateMachine : public raft::StateMachine {
public:
    KVStateMachine(StorageEngine& s,  raft::Channel& leaderChannel, raft::Channel& followerChannel, raft::Raft& r);
    KVStateMachine(const KVStateMachine&) = delete;
    KVStateMachine& operator=(const KVStateMachine&) = delete;
    KVStateMachine(KVStateMachine&&) = delete;
    KVStateMachine& operator=(KVStateMachine&&) = delete;
    void applyCommand(raft::Command* command) override;
    void consumeChannel() override;
    void snapshot() override;
    void restore(const std::string& snapshot) override;
    State handleGet(Key key, std::chrono::system_clock::time_point t);
    State handleSet(Key key, Value value, std::chrono::system_clock::time_point t);
    State handleErase(Key key, std::chrono::system_clock::time_point t);
    State handleSize(std::chrono::system_clock::time_point t);
    State get(Key key);
    State set(Key key, Value value);
    State erase(Key key);
    State size();
    ~KVStateMachine();
private:
    StorageEngine& storageEngine;
    raft::Channel& leader;
    raft::Channel& follower;
    raft::Raft& raft;
    std::thread t;
};

} // namespace zdb

#endif // KV_STATEMACHINE_H
