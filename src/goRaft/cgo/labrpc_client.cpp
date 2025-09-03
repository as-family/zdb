#include "labrpc_client.hpp"
#include <iostream>
#include <cstring>

LabrpcRaftClient::LabrpcRaftClient(int peer_id, const zdb::RetryPolicy& policy, labrpc_call_func call_func)
    : peer_id_(peer_id), policy_(policy), call_func_(call_func) {}

template<typename Req, typename Rep>
std::optional<std::monostate> LabrpcRaftClient::call(const std::string& method_name,
                                   grpc::Status (raft::proto::Raft::Stub::*)(grpc::ClientContext*, const Req&, Rep*),
                                   const Req& request, 
                                   Rep& reply) {
    
    if (!call_func_) {
        std::cerr << "labrpc callback function not set" << std::endl;
        return std::nullopt;
    }
    
    // Serialize protobuf request
    std::string serialized_request;
    if (!request.SerializeToString(&serialized_request)) {
        std::cerr << "Failed to serialize request" << std::endl;
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
        std::cerr << "Unknown method: " << method_name << std::endl;
        return std::nullopt;
    }
    
    // Call through labrpc
    int result = call_func_(peer_id_, service_method.c_str(),
                           serialized_request.data(), serialized_request.size(),
                           &serialized_reply[0], serialized_reply.size());
    
    if (result > 0) {
        // Resize to actual reply size and deserialize
        serialized_reply.resize(result);
        if (reply.ParseFromString(serialized_reply)) {
            return std::optional<std::monostate>{std::monostate{}};
        } else {
            std::cerr << "Failed to parse reply" << std::endl;
        }
    } else {
        std::cerr << "labrpc call failed, result: " << result << std::endl;
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
