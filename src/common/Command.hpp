#ifndef COMMAND_H
#define COMMAND_H

#include "raft/Command.hpp"
#include "raft/StateMachine.hpp"
#include "server/KVStoreServer.hpp"
#include "proto/types.pb.h"

namespace zdb {

struct Get : public raft::Command {
    std::string key;

    Get(const std::string& k)
        : key(k) {}
    Get(const proto::Command& cmd) {
        key = cmd.key().data();
    }

    std::string serialize() const override {
        auto c = proto::Command {};
        c.set_op("get");
        c.mutable_key()->set_data(key);
        return c.SerializeAsString();
    }

    void apply(raft::StateMachine* stateMachine) override {
        auto* kvState = dynamic_cast<zdb::KVStoreServiceImpl*>(stateMachine);
        if (kvState) {
            kvState->get(nullptr, nullptr, nullptr);
        }
    }
};

} // namespace zdb

#endif // COMMAND_H
