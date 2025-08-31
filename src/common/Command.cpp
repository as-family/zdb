#include "common/Command.hpp"
#include "proto/types.pb.h"
#include <google/protobuf/any.pb.h>

namespace zdb
{

raft::Command* commandFactory(const std::string& s) {
    auto cmd = zdb::proto::Command {};
    if (!cmd.ParseFromString(s)) {
        throw std::invalid_argument{"commandFactory: deserialization failed"};
    }
    if (cmd.op() == "get") {
        return new Get{cmd};
    }
    if (cmd.op() == "set") {
        return new Set{cmd};
    }
    if (cmd.op() == "erase") {
        return new Erase{cmd};
    }
    if (cmd.op() == "size") {
        return new Size{cmd};
    }
    throw std::invalid_argument{"commandFactory: unknown op: " + cmd.op()};
}

} // namespace zdb
