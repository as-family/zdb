#ifndef KV_STATEMACHINE_H
#define KV_STATEMACHINE_H

#include "raft/StateMachine.hpp"
#include "common/Types.hpp"
#include "raft/Channel.hpp"
#include <thread>
#include "interface/StorageEngine.hpp"
#include "raft/Raft.hpp"
#include <chrono>
#include "common/Command.hpp"
#include <memory>

namespace zdb {

class KVStateMachine : public raft::StateMachine {
public:
    KVStateMachine(StorageEngine& s,  raft::Channel& leaderChannel, raft::Channel& followerChannel, raft::Raft& r);
    KVStateMachine(const KVStateMachine&) = delete;
    KVStateMachine& operator=(const KVStateMachine&) = delete;
    KVStateMachine(KVStateMachine&&) = delete;
    KVStateMachine& operator=(KVStateMachine&&) = delete;
    std::unique_ptr<raft::State> applyCommand(raft::Command& command) override;
    void consumeChannel() override;
    void snapshot() override;
    void restore(const std::string& snapshot) override;
    State handleGet(Get c, std::chrono::system_clock::time_point t);
    State handleSet(Set c, std::chrono::system_clock::time_point t);
    State handleErase(Erase c, std::chrono::system_clock::time_point t);
    State handleSize(Size c, std::chrono::system_clock::time_point t);
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
    std::thread consumerThread;
};

} // namespace zdb

#endif // KV_STATEMACHINE_H
