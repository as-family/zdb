#include "labrpc_client.hpp"
#include <iostream>
#include <cstring>

LabrpcRaftClient::LabrpcRaftClient(int caller_id, int peer_id, const zdb::RetryPolicy& policy, labrpc_call_func call_func)
    : caller_id_(caller_id), peer_id_(peer_id), policy_(policy), call_func_(call_func), circuitBreaker{policy_} {}

template<typename Req, typename Rep>
std::optional<std::monostate> LabrpcRaftClient::call(const std::string& method_name,
                                   grpc::Status (raft::proto::Raft::Stub::*)(grpc::ClientContext*, const Req&, Rep*),
                                   const Req& request, 
                                   Rep& reply) {
    // std::cerr << "LabrpcRaftClient::call to peer " << peer_id_ << " method " << method_name << "\n";
    if (!call_func_) {
        return std::nullopt;
    }
    
    // Serialize protobuf request
    std::string serialized_request;
    if (!request.SerializeToString(&serialized_request)) {
        return std::nullopt;
    }
    
    // Prepare reply buffer
    std::string serialized_reply;
    serialized_reply.resize(1024); // Initial buffer size
    
    // Determine service method name
    std::string service_method;
    if (method_name == "requestVote" || 
        std::is_same_v<Req, raft::proto::RequestVoteArg>) {
        service_method = "CppRaft.RequestVote";
    } else if (method_name == "appendEntries" || 
               std::is_same_v<Req, raft::proto::AppendEntriesArg>) {
        service_method = "CppRaft.AppendEntries";
    } else {
        return std::nullopt;
    }
    
    // Call through labrpc
    int result = -1;
    auto f = [&]() {
        result = call_func_(caller_id_, peer_id_, service_method.c_str(),
                          serialized_request.data(), serialized_request.size(),
                          &serialized_reply[0], serialized_reply.size());
        return result > 0 ? grpc::Status::OK : grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "labrpc call failed");
    };
    circuitBreaker.call(method_name, f);

    if (result > 0) {
        // Resize to actual reply size and deserialize
        serialized_reply.resize(result);
        if (reply.ParseFromString(serialized_reply)) {
            return std::optional<std::monostate>{std::monostate{}};
        }
    }
    
    return std::nullopt;
}

// Explicit template instantiations
template std::optional<std::monostate> LabrpcRaftClient::call<raft::proto::RequestVoteArg, raft::proto::RequestVoteReply>(
    const std::string&, grpc::Status (raft::proto::Raft::Stub::*)(grpc::ClientContext*, const raft::proto::RequestVoteArg&, raft::proto::RequestVoteReply*),
    const raft::proto::RequestVoteArg&, raft::proto::RequestVoteReply&);
    
template std::optional<std::monostate> LabrpcRaftClient::call<raft::proto::AppendEntriesArg, raft::proto::AppendEntriesReply>(
    const std::string&, grpc::Status (raft::proto::Raft::Stub::*)(grpc::ClientContext*, const raft::proto::AppendEntriesArg&, raft::proto::AppendEntriesReply*),
    const raft::proto::AppendEntriesArg&, raft::proto::AppendEntriesReply&);
