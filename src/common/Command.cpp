#include "common/Command.hpp"
#include "proto/types.pb.h"
#include <google/protobuf/any.pb.h>

namespace zdb
{

raft::Command* commandFactory(const std::string& s) {
    auto cmd = zdb::proto::Command {};
    google::protobuf::Any any;
    if (!any.ParseFromString(s) || !any.UnpackTo(&cmd)) {
        throw std::invalid_argument{"commandFactory: deserialization failed"};
    }
    if (cmd.op() == "get") {
        return new Get{cmd};
    }
    if (cmd.op() == "set") {
        return new Set{cmd};
    }
    throw std::invalid_argument{"commandFactory: unknown op: " + cmd.op()};
}

} // namespace zdb
