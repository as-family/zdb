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

extern "C" int go_invoke_callback(uintptr_t handle, int p, char* f, void* args, int args_len, void* reply, int reply_len);

class GoRPCClient {
public:
    GoRPCClient(int ii, std::string a, const zdb::RetryPolicy p, uintptr_t h);
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
            len = go_invoke_callback(handle, i, name.data(), r.data(), r.size(), p.data(), p.size());
            if (len <= 0) {
                return grpc::Status{grpc::StatusCode::DEADLINE_EXCEEDED, "labrpc failed"};
            } else {
                return grpc::Status::OK;
            }
        };
        zdb::CircuitBreaker circuitBreaker{policy};
        breakers.push_back(std::ref(circuitBreaker));
        auto status = circuitBreaker.call(name, f);
        if (!status.back().ok()) {
            return std::nullopt;
        }
        if (len <= 0) {
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
    std::vector<std::reference_wrapper<zdb::CircuitBreaker>> breakers;
};

#endif // GO_RPC_CLIENT_HPP
