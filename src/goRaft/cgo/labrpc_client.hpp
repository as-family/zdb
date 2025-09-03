#ifndef LABRPC_CLIENT_HPP
#define LABRPC_CLIENT_HPP

#include "proto/raft.grpc.pb.h"
#include "common/RetryPolicy.hpp"
#include <optional>
#include <string>

// Forward declare the Go callback function type
extern "C" {
    typedef int (*labrpc_call_func)(int peer_id, const char* service_method, 
                                   const void* args, int args_size,
                                   void* reply, int reply_size);
}

// Adapter that implements the same interface as RPCService but uses labrpc
class LabrpcRaftClient {
public:
    LabrpcRaftClient(int peer_id, const zdb::RetryPolicy& policy, labrpc_call_func call_func);
    
    // Same interface as RPCService::call
    template<typename Req, typename Rep>
    std::optional<std::monostate> call(const std::string& method_name, 
                       grpc::Status (raft::proto::Raft::Stub::*)(grpc::ClientContext*, const Req&, Rep*),
                       const Req& request, 
                       Rep& reply);
                       
    bool available() const { return true; } // labrpc handles availability
    
private:
    int peer_id_;
    zdb::RetryPolicy policy_;
    labrpc_call_func call_func_;
};

#endif // LABRPC_CLIENT_HPP
