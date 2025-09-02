#include "raft_wrapper.h"
#include "raft/RaftImpl.hpp"
#include "raft/SyncChannel.hpp"
#include "raft/RaftServiceImpl.hpp"
#include "server/RPCServer.hpp"
#include "common/RetryPolicy.hpp"
#include "common/Types.hpp"
#include "common/RPCService.hpp"
#include "proto/raft.grpc.pb.h"
#include <memory>
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

// Forward declarations
namespace grpc {
    class Status;
    class ClientContext;
}

// Use the production RPC service for real Raft communication
using RaftRPCService = zdb::RPCService<raft::proto::Raft>;
using RaftServer = zdb::RPCServer<raft::RaftServiceImpl>;

struct RaftHandle {
    std::unique_ptr<raft::RaftImpl<RaftRPCService>> raft_impl;
    std::unique_ptr<raft::SyncChannel> service_channel;
    std::unique_ptr<raft::SyncChannel> follower_channel;
    std::unordered_map<std::string, std::unique_ptr<RaftRPCService>> clients;
    
    // Server components for receiving Raft RPCs
    std::unique_ptr<raft::RaftServiceImpl> raft_service;
    std::unique_ptr<RaftServer> rpc_server;
    
    std::string self_id;
    std::string listen_address;  // Address this server listens on
    int me;
    std::atomic<bool> killed{false};
    std::chrono::steady_clock::time_point creation_time;
    
    // Apply message queue for Go communication
    std::queue<ApplyMsg> apply_queue;
    std::mutex apply_mutex;
    std::condition_variable apply_cv;
};

extern "C" {

RaftHandle* raft_create(char** servers, int num_servers, int me, char* persister_id) {
    try {
        auto handle = std::make_unique<RaftHandle>();
        handle->me = me;
        handle->self_id = servers[me];
        handle->creation_time = std::chrono::steady_clock::now();
        
        // Set listen address based on server index
        handle->listen_address = "localhost:" + std::to_string(9000 + me);
        
        // Create service and follower channels
        handle->service_channel = std::make_unique<raft::SyncChannel>();
        handle->follower_channel = std::make_unique<raft::SyncChannel>();
        
        // Create RPC clients for communicating with other servers
        for (int i = 0; i < num_servers; i++) {
            if (i != me) {
                std::string peer_id = servers[i];
                std::string peer_address = "localhost:" + std::to_string(9000 + i);
                auto retry_policy = zdb::RetryPolicy(
                    std::chrono::milliseconds(10),      // base delay
                    std::chrono::milliseconds(50),      // max delay
                    std::chrono::milliseconds(60),      // reset timeout
                    10,                                  // failure threshold
                    10,                                  // services to try
                    std::chrono::milliseconds(4),       // rpc timeout
                    std::chrono::milliseconds(4)        // channel timeout
                );
                auto client = std::make_unique<RaftRPCService>(peer_address, retry_policy);
                handle->clients[peer_id] = std::move(client);
            }
        }
        
        // Create RaftImpl with the RPC clients
        std::vector<std::string> peer_ids;
        for (int i = 0; i < num_servers; i++) {
            peer_ids.push_back(servers[i]);
        }
        
        auto retry_policy = zdb::RetryPolicy(
            std::chrono::milliseconds(10),      // base delay
            std::chrono::milliseconds(50),      // max delay
            std::chrono::milliseconds(60),      // reset timeout
            10,                                  // failure threshold
            10,                                  // services to try
            std::chrono::milliseconds(4),       // rpc timeout
            std::chrono::milliseconds(4)        // channel timeout
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
        
        // Create server-side RaftServiceImpl for receiving RPCs  
        handle->raft_service = std::make_unique<raft::RaftServiceImpl>(*handle->raft_impl);
        
        // Start gRPC server to receive Raft RPCs from peers
        handle->rpc_server = std::make_unique<RaftServer>(
            handle->listen_address, 
            *handle->raft_service
        );
        
        return handle.release();
    } catch (const std::exception& e) {
        std::cerr << "Error creating Raft: " << e.what() << std::endl;
        return nullptr;
    }
}

void raft_destroy(RaftHandle* handle) {
    if (handle) {
        handle->killed.store(true);
        
        // Shutdown the gRPC server first
        if (handle->rpc_server) {
            handle->rpc_server->shutdown();
        }
        
        // Clean up Raft implementation
        if (handle->raft_impl) {
            handle->raft_impl.reset();
        }
        
        // Clean up clients
        handle->clients.clear();
        
        // Clean up server components
        handle->rpc_server.reset();
        handle->raft_service.reset();
        
        // Clean up channels
        handle->service_channel.reset();
        handle->follower_channel.reset();
        
        delete handle;
    }
}

void raft_kill(RaftHandle* handle) {
    if (handle && handle->raft_impl) {
        handle->killed = true;
        handle->raft_impl->kill();
        handle->apply_cv.notify_all();
    }
}

int raft_start(RaftHandle* handle, char* command, int* index, int* term, int* is_leader) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {
        std::string cmd(command);
        bool success = handle->raft_impl->start(cmd);
        
        // Get current state
        *term = static_cast<int>(handle->raft_impl->getCurrentTerm());
        *is_leader = (handle->raft_impl->getRole() == raft::Role::Leader) ? 1 : 0;
        *index = success ? static_cast<int>(handle->raft_impl->log().lastIndex()) : -1;
        
        return success ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int raft_get_state(RaftHandle* handle, int* term, int* is_leader) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {
        *term = static_cast<int>(handle->raft_impl->getCurrentTerm());
        auto role = handle->raft_impl->getRole();
        *is_leader = (role == raft::Role::Leader) ? 1 : 0;
        
        std::cerr << "raft_get_state: " << handle->self_id 
                  << " term=" << *term 
                  << " is_leader=" << *is_leader 
                  << " role=" << static_cast<int>(role) << std::endl;
        
        return 1;
    } catch (...) {
        return 0;
    }
}

int raft_persist_bytes(RaftHandle* handle) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return 0;
    }
    
    try {
        // For minimal implementation, return 0
        // In full implementation, this would return actual persisted bytes
        return 0;
    } catch (...) {
        return 0;
    }
}

void raft_snapshot(RaftHandle* handle, int index, char* snapshot, int snapshot_len) {
    if (!handle || !handle->raft_impl || handle->killed) {
        return;
    }
    
    try {
        // For minimal implementation, do nothing
        // In full implementation, this would handle snapshots
        (void)index;
        (void)snapshot;
        (void)snapshot_len;
    } catch (...) {
        // Ignore errors for minimal implementation
    }
}

int raft_receive_apply_msg(RaftHandle* handle, ApplyMsg* msg, int timeout_ms) {
    if (!handle || handle->killed) {
        return 0;
    }
    
    try {
        std::unique_lock<std::mutex> lock(handle->apply_mutex);
        
        // Wait for message or timeout
        if (handle->apply_queue.empty()) {
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (!handle->apply_cv.wait_for(lock, timeout, [handle] { 
                return !handle->apply_queue.empty() || handle->killed; 
            })) {
                return 0; // Timeout
            }
        }
        
        if (handle->killed || handle->apply_queue.empty()) {
            return 0;
        }
        
        // Get message from queue
        *msg = handle->apply_queue.front();
        handle->apply_queue.pop();
        
        return 1;
    } catch (...) {
        return 0;
    }
}

} // extern "C"
