// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * ZDB a distributed, fault-tolerant database.
 * Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef GO_RPC_CLIENT_HPP
#define GO_RPC_CLIENT_HPP

#include "common/CircuitBreaker.hpp"
#include "common/RetryPolicy.hpp"
#include <optional>
#include <string>
#include <mutex>
#include <atomic>
#include <memory>
#include "common/TypesMap.hpp"

extern "C" int go_invoke_callback(uintptr_t handle, int p, char* f, void* args, int args_len, void* reply, int reply_len);

class GoRPCClient {
public:
    GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h, std::atomic<bool>& sc);
    template<typename Req, typename Rep = zdb::map_to_t<Req>>
    std::optional<Rep> call(std::string name, Req& request) {
        std::string p;
        if (name == "requestVote") {
            name = "Raft.RequestVote";
            p.resize(64);
        } else if (name == "appendEntries") {
            name = "Raft.AppendEntries";
            p.resize(64);
        } else if (name == "installSnapshot") {
            name = "Raft.InstallSnapshot";
            p.resize(65536);
        } else {
            throw std::invalid_argument{"Unknown function " + name};
        }
        auto& reqMsg = static_cast<google::protobuf::Message&>(request);
        auto r = reqMsg.SerializeAsString();
        auto len = 0;
        auto f = [&]() -> grpc::Status {
            std::string r_copy = r; // Make a copy to ensure the data pointer remains valid
            std::string p_copy = p; // Make a copy to ensure the data pointer remains valid
            len = go_invoke_callback(handle, peerId, name.data(), r_copy.data(), r_copy.size(), p_copy.data(), p_copy.size());
            if (len < 0) {
                return grpc::Status{grpc::StatusCode::DEADLINE_EXCEEDED, "labrpc failed"};
            } else {
                p = std::move(p_copy);
                return grpc::Status::OK;
            }
        };
        auto status = circuitBreaker.call(name, f);
        if (!status.back().ok()) {
            return std::nullopt;
        }
        if (len < 0) {
            return std::nullopt;
        }
        p.resize(len);
        if constexpr (std::is_same_v<Rep, raft::RequestVoteReply>) {
            auto reply = raft::proto::RequestVoteReply{};
            reply.ParseFromString(p);
            return raft::RequestVoteReply{reply};
        } else if constexpr (std::is_same_v<Rep, raft::AppendEntriesReply>) {
            auto reply = raft::proto::AppendEntriesReply{};
            reply.ParseFromString(p);
            return raft::AppendEntriesReply{reply};
        } else if constexpr (std::is_same_v<Rep, raft::InstallSnapshotReply>) {
            auto reply = raft::proto::InstallSnapshotReply{};
            reply.ParseFromString(p);
            return raft::InstallSnapshotReply{reply};
        }
        return std::nullopt;
    }
private:
    int peerId;
    zdb::RetryPolicy policy;
    uintptr_t handle;
    std::atomic<bool>& stopCalls;
    zdb::CircuitBreaker circuitBreaker;
};

#endif // GO_RPC_CLIENT_HPP
