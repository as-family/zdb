#include "raft_wrapper.h"
#include "raft/RaftImpl.hpp"
#include "raft/SyncChannel.hpp"
#include "labrpc_client.hpp"  // Replace RPCService include
#include "common/RetryPolicy.hpp"
#include "common/Types.hpp"
#include "proto/raft.grpc.pb.h"
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <type_traits>
#include <cstring>

// Use labrpc client instead of gRPC
using RaftRPCService = LabrpcRaftClient;

// Global callback function set by Go
static labrpc_call_func g_labrpc_call_func = nullptr;

struct RaftHandle {
    std::unique_ptr<raft::RaftImpl<RaftRPCService>> raft_impl;
    std::unique_ptr<raft::SyncChannel> service_channel;
    std::unique_ptr<raft::SyncChannel> follower_channel;
    std::unordered_map<std::string, std::unique_ptr<RaftRPCService>> clients;
    
    // Remove gRPC server components - labrpc handles this
    // std::unique_ptr<raft::RaftServiceImpl> raft_service;
    // std::unique_ptr<RaftServer> rpc_server;
    
    std::string self_id;
    int me;
    std::atomic<bool> killed{false};
    std::chrono::steady_clock::time_point creation_time;
    
    // Apply message queue for Go communication
    std::queue<ApplyMsg> apply_queue;
    std::mutex apply_mutex;
    std::condition_variable apply_cv;
};

extern "C" {

// Set the labrpc callback function from Go
void raft_set_labrpc_callback(labrpc_call_func func) {
    g_labrpc_call_func = func;
}

// RPC handlers called by Go labrpc framework
int raft_request_vote_handler(RaftHandle* handle, const char* args_data, int args_size,
                             char* reply_data, int reply_size) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {
        
        // Deserialize request
        raft::proto::RequestVoteArg args;
        std::string args_str(args_data, args_size);
        if (!args.ParseFromString(args_str)) {
            return 0;
        }
        
        // Call C++ Raft implementation
        auto reply = handle->raft_impl->requestVoteHandler(args);
        
        // Convert to protobuf reply
        raft::proto::RequestVoteReply reply_proto;
        reply_proto.set_votegranted(reply.voteGranted);
        reply_proto.set_term(reply.term);
        
        // Serialize reply
        std::string reply_str;
        if (!reply_proto.SerializeToString(&reply_str)) {
            return 0;
        }
        
        // Copy to output buffer
        if (reply_str.size() <= static_cast<size_t>(reply_size)) {
            memcpy(reply_data, reply_str.data(), reply_str.size());
            return reply_str.size();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception in request_vote_handler: " << e.what() << std::endl;
        return 0;
    }
}

int raft_append_entries_handler(RaftHandle* handle, const char* args_data, int args_size,
                               char* reply_data, int reply_size) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {        
        // Deserialize request
        raft::proto::AppendEntriesArg args_proto;
        std::string args_str(args_data, args_size);
        if (!args_proto.ParseFromString(args_str)) {
            return 0;
        }
        
        // Convert to internal format
        raft::AppendEntriesArg args{args_proto};
        
        // Call C++ Raft implementation
        auto reply = handle->raft_impl->appendEntriesHandler(args);
        
        // Convert reply to protobuf
        raft::proto::AppendEntriesReply reply_proto;
        reply_proto.set_success(reply.success);
        reply_proto.set_term(reply.term);
        
        // Serialize reply
        std::string reply_str;
        if (!reply_proto.SerializeToString(&reply_str)) {
            return 0;
        }
        
        // Copy to output buffer
        if (reply_str.size() <= static_cast<size_t>(reply_size)) {
            memcpy(reply_data, reply_str.data(), reply_str.size());
            return reply_str.size();
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception in append_entries_handler: " << e.what() << std::endl;
        return 0;
    }
}

RaftHandle* raft_create(char** servers, int num_servers, int me, char* persister_id) {
    try {        
        auto handle = std::make_unique<RaftHandle>();
        handle->me = me;
        handle->self_id = "peer_" + std::to_string(me);
        handle->creation_time = std::chrono::steady_clock::now();
        
        // Create service and follower channels
        handle->service_channel = std::make_unique<raft::SyncChannel>();
        handle->follower_channel = std::make_unique<raft::SyncChannel>();
        
        // Create labrpc clients for communicating with other servers
        for (int i = 0; i < num_servers; i++) {
            if (i != me) {
                std::string peer_id = "peer_" + std::to_string(i);
                auto retry_policy = zdb::RetryPolicy(
                    std::chrono::milliseconds(1),
                    std::chrono::milliseconds(20),
                    std::chrono::milliseconds(25),
                    10, 10,
                    std::chrono::milliseconds(4),
                    std::chrono::milliseconds(4)
                );
                auto client = std::make_unique<LabrpcRaftClient>(me, i, retry_policy, g_labrpc_call_func);
                handle->clients[peer_id] = std::move(client);
            }
        }
        
        // Create peer IDs for RaftImpl
        std::vector<std::string> peer_ids;
        for (int i = 0; i < num_servers; i++) {
            peer_ids.push_back("peer_" + std::to_string(i));
        }
        
        auto retry_policy = zdb::RetryPolicy(
            std::chrono::milliseconds(1),
            std::chrono::milliseconds(20),
            std::chrono::milliseconds(25),
            10, 10,
            std::chrono::milliseconds(4),
            std::chrono::milliseconds(4)
        );
        
        handle->raft_impl = std::make_unique<raft::RaftImpl<RaftRPCService>>(
            peer_ids,
            handle->self_id,
            *handle->service_channel,
            *handle->follower_channel,
            retry_policy,
            [clients = &handle->clients](const std::string& peer_id, zdb::RetryPolicy) -> RaftRPCService& {
                auto it = clients->find(peer_id);
                if (it != clients->end()) {
                    return *it->second;
                }
                throw std::runtime_error("Client not found for peer: " + peer_id);
            }
        );
        
        // No gRPC server needed - labrpc handles this

        return handle.release();
    } catch (const std::exception& e) {
        std::cerr << "Error creating Raft: " << e.what() << std::endl;
        return nullptr;
    }
}

void raft_destroy(RaftHandle* handle) {
    if (handle) {
        handle->killed = true;
        std::cerr << "Destroying RaftHandle for " << handle->self_id << "\n";
        delete handle;
    }
}

void raft_kill(RaftHandle* handle) {
    if (handle) {
        handle->killed = true;
        std::cerr << "Killing RaftHandle for " << handle->self_id << "\n";
        // Properly destruct the RaftImpl to stop timers and cleanup
        handle->raft_impl.reset();
    }
}

int raft_get_state(RaftHandle* handle, int* term, int* is_leader) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {
        auto current_term = handle->raft_impl->getCurrentTerm();
        auto role = handle->raft_impl->getRole();
        
        *term = current_term;
        *is_leader = (role == raft::Role::Leader) ? 1 : 0;
        
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error getting state: " << e.what() << std::endl;
        return 0;
    }
}

int raft_start(RaftHandle* handle, char* command, int* index, int* term, int* is_leader) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {
        std::string cmd_str(command);
        
        // Get current term and role before starting
        auto current_term = handle->raft_impl->getCurrentTerm();
        auto role = handle->raft_impl->getRole();
        bool isLeader = (role == raft::Role::Leader);
        
        if (isLeader) {
            // If we're the leader, try to start the command
            bool success = handle->raft_impl->start(cmd_str);
            if (success) {
                // For now, we'll use the log's last index + 1 as the index
                // This is a simplification - in a real implementation, you'd get the actual index
                auto& log = handle->raft_impl->log();
                *index = log.lastIndex() + 1; // Next index where command will be placed
                *term = current_term;
                *is_leader = 1;
                return 1;
            }
        }
        
        *index = -1;
        *term = current_term;
        *is_leader = isLeader ? 1 : 0;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error starting command: " << e.what() << std::endl;
        return 0;
    }
}

int raft_persist_bytes(RaftHandle* handle) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    // TODO: Implement persistence
    return 0;
}

void raft_snapshot(RaftHandle* handle, int index, char* snapshot, int snapshot_len) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return;
    }
    
    // TODO: Implement snapshotting
}

int raft_receive_apply_msg(RaftHandle* handle, ApplyMsg* msg, int timeout_ms) {
    if (!handle || handle->killed) {
        return 0;
    }
    
    std::unique_lock<std::mutex> lock(handle->apply_mutex);
    
    if (timeout_ms > 0) {
        auto timeout = std::chrono::milliseconds(timeout_ms);
        if (!handle->apply_cv.wait_for(lock, timeout, [handle] { return !handle->apply_queue.empty(); })) {
            return 0; // Timeout
        }
    } else {
        handle->apply_cv.wait(lock, [handle] { return !handle->apply_queue.empty(); });
    }
    
    if (!handle->apply_queue.empty()) {
        *msg = handle->apply_queue.front();
        handle->apply_queue.pop();
        return 1;
    }
    
    return 0;
}

} // extern "C"
