#include "RaftTestFramework/RaftTestFramework.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include "KVTestFramework/NetworkConfig.hpp"
#include "KVTestFramework/KVTestFramework.hpp"
#include "raft/Raft.hpp"
#include "raft/Channel.hpp"
#include "raft/RaftImpl.hpp"
#include <algorithm>
#include "common/Command.hpp"
#include "raft/Command.hpp"
#include "common/Command.hpp"
#include "proto/types.pb.h"

raft::Command* commandFactory(const std::string& s) {
    auto cmd = zdb::proto::Command {};
    google::protobuf::Any any;
    if (any.ParseFromString(s)) {
        if (any.UnpackTo(&cmd)) {
            if (cmd.op() == "get") {
                return new zdb::Get {cmd};
            } else if (cmd.op() == "put") {
                return new zdb::Set {cmd};
            } else {
                return nullptr;
            }
        }
    }
}

RAFTTestFramework::RAFTTestFramework(
        std::vector<std::tuple<std::string, std::string, NetworkConfig>> c
    ) : config(std::move(c)) {
    std::vector<std::string> proxies {config.size()};
    std::transform(
        config.begin(),
        config.end(),
        proxies.begin(),
        [](const auto& tup) { return std::get<1>(tup); }
    );
    for (auto& [target, proxy, cfg] : config) {
        channels.emplace(std::piecewise_construct, std::forward_as_tuple(target), std::forward_as_tuple());
        rafts.emplace(std::piecewise_construct, std::forward_as_tuple(target), std::forward_as_tuple(proxies, proxy, channels[target], commandFactory));
        kvTests.emplace(std::piecewise_construct, std::forward_as_tuple(target), std::forward_as_tuple(proxy, target, cfg));
    }
}

RAFTTestFramework::~RAFTTestFramework() = default;


