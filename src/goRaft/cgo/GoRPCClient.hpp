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
#include <unordered_map>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <cstdint>
#include "grpc/grpc.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>

extern "C" int go_invoke_callback(uintptr_t handle, int p, char* f, void* args, int args_len, void* reply, int reply_len);

class GoRPCClient {
public:
    GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h);
    void stop();
    ~GoRPCClient();
    template<typename Req, typename Rep>
    std::optional<std::monostate> call(std::string name, Req& request, Rep& reply) {
        if (name == "requestVote") {
            name = "Raft.RequestVote";
        } else if (name == "appendEntries") {
            name = "Raft.AppendEntries";
        } else {
            throw std::invalid_argument{"Unknown function " + name};
        }
        std::string r;
        if (!request.SerializeToString(&r)) {
            throw std::runtime_error("failed to serialize request");
        }
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
        auto circuitBreaker = std::make_unique<zdb::CircuitBreaker>(policy);
        std::unique_lock lock{m};
        breakers.push_back(std::move(circuitBreaker));
        auto& breaker = *breakers.back();  
        lock.unlock();
        auto status = breaker.call(name, f);
        if (!status.back().ok()) {
            return std::nullopt;
        }
        if (len < 0) {
            return std::nullopt;
        }
        p.resize(len);
        if (reply.ParseFromString(p)) {
            return std::optional<std::monostate>{std::monostate{}};
        }
        return std::nullopt;
    }
private:
    int i;
    std::string address;
    zdb::RetryPolicy policy;
    uintptr_t handle;
    std::vector<std::unique_ptr<zdb::CircuitBreaker>> breakers;
    std::mutex m;
};

#endif // GO_RPC_CLIENT_HPP
