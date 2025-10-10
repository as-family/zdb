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
    void stop();
    template<typename Req, typename Rep = zdb::map_to_t<Req>>
    std::optional<Rep> call(std::string name, Req& request) {
        if (name == "requestVote") {
            name = "Raft.RequestVote";
        } else if (name == "appendEntries") {
            name = "Raft.AppendEntries";
        } else {
            throw std::invalid_argument{"Unknown function " + name};
        }
        auto& reqMsg = static_cast<google::protobuf::Message&>(request);
        auto r = reqMsg.SerializeAsString();
        auto p = std::string{};
        p.resize(1024);
        auto len = 0;
        auto f = [&]() -> grpc::Status {
            std::string r_copy = r; // Make a copy to ensure the data pointer remains valid
            std::string p_copy = p; // Make a copy to ensure the data pointer remains valid
            len = go_invoke_callback(handle, i, name.data(), r_copy.data(), r_copy.size(), p_copy.data(), p_copy.size());
            if (len < 0) {
                return grpc::Status{grpc::StatusCode::DEADLINE_EXCEEDED, "labrpc failed"};
            } else {
                p = std::move(p_copy);
                return grpc::Status::OK;
            }
        };
        // std::cerr << "calling " << name << "\n";
        auto t = std::chrono::system_clock::now();
        auto status = circuitBreaker.call(name, f);
        std::cerr << "called " << name << " in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t) << "\n";
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
        } else {
            auto reply = raft::proto::AppendEntriesReply{};
            reply.ParseFromString(p);
            // std::cerr << "reply: " << reply.DebugString() << "\n";
            return raft::AppendEntriesReply{reply};
        }
        return std::nullopt;
    }
private:
    int i;
    std::mutex m;
    std::string address;
    zdb::RetryPolicy policy;
    uintptr_t handle;
    std::atomic<bool>& stopCalls;
    zdb::CircuitBreaker circuitBreaker;
};

#endif // GO_RPC_CLIENT_HPP
