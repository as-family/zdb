#ifndef KV_STATEMACHINE_H
#define KV_STATEMACHINE_H

#include "raft/StateMachine.hpp"
#include "common/Types.hpp"
#include "raft/Channel.hpp"
#include <thread>
#include "interface/StorageEngine.hpp"
#include "raft/Raft.hpp"

namespace zdb {

class KVStateMachine : public raft::StateMachine {
public:
    KVStateMachine(StorageEngine* s,  raft::Channel* leaderChannel, raft::Channel* followerChannel, raft::Raft* raft);
    raft::State* applyCommand(raft::Command* command) override;
    void consumeChannel() override;
    void snapshot() override;
    void restore(const std::string& snapshot) override;
    raft::State* handleGet(Key key);
    raft::State* handleSet(Key key, Value value);
    raft::State* handleErase(Key key);
    raft::State* handleSize();
    raft::State* get(Key key);
    raft::State* set(Key key, Value value);
    raft::State* erase(Key key);
    raft::State* size();
    ~KVStateMachine();
private:
    StorageEngine* storageEngine;
    raft::Channel* leader;
    raft::Channel* follower;
    raft::Raft* raft;
    std::thread t;
};

} // namespace zdb

#endif // KV_STATEMACHINE_H
