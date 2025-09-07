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
#include "raft_wrapper.hpp"
#include "raft/Raft.hpp"
#include "raft/RaftImpl.hpp"
#include "raft/Channel.hpp"
#include "raft/SyncChannel.hpp"
#include "proto/raft.pb.h"
#include "GoRPCClient.hpp"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdint>

struct RaftHandle {
    int id;
    int servers;
    std::string selfId;
    std::vector<std::string> peers;
    raft::Channel* serviceChannel;
    raft::Channel* followerChannel;
    zdb::RetryPolicy policy;
    uintptr_t callback;
    std::unordered_map<std::string, int> peerIds;
    std::unordered_map<std::string, std::unique_ptr<GoRPCClient>> clients;
    std::unique_ptr<raft::RaftImpl<GoRPCClient>> raft;
};


extern "C" {

void kill_raft(RaftHandle* h) {
    // std::cerr << "C++: Killing Raft instance, handle=" << h << "\n";
    if (!h) {
        // std::cerr << "C++: Handle is null, nothing to kill\n";
        return;
    }
    
    // Add some basic validation
    // std::cerr << "C++: Checking handle validity\n";
    if (h->raft) {
        // std::cerr << "C++: Raft instance exists, signaling kill\n";
        // Properly signal the raft instance to stop its operations
        h->raft->kill();
        // std::cerr << "C++: Raft instance kill signal sent\n";
        
        // Give threads a moment to recognize the kill signal
        std::this_thread::sleep_for(std::chrono::milliseconds{10L});
        
        // Reset the unique_ptr, which will automatically call the destructor
        h->raft.reset();
        // std::cerr << "C++: Raft instance destroyed\n";
    } else {
        // std::cerr << "C++: Raft instance was already null\n";
    }
    
    // std::cerr << "C++: Raft instance killed\n";
    if (h->serviceChannel) {
        h->serviceChannel->close();
        delete h->serviceChannel;
        h->serviceChannel = nullptr;
    }
    // std::cerr << "C++: Service channel closed\n";
    if (h->followerChannel) {
        h->followerChannel->close();
        delete h->followerChannel;
        h->followerChannel = nullptr;
    }
}

RaftHandle* create_raft(int id, int servers, uintptr_t cb) {
    std::vector<std::string> peers;
    std::unordered_map<std::string, int> ids;
    std::string selfId = "peer_" + std::to_string(id);
    for (int i = 0; i < servers; ++i) {
        auto a = "peer_" + std::to_string(i);
        peers.push_back(a);
        ids[a] = i;
    }
    auto handle = new RaftHandle{
        id,
        servers,
        selfId,
        peers,
        new raft::SyncChannel(),
        new raft::SyncChannel(),
        zdb::RetryPolicy(
            std::chrono::milliseconds{2L},
            std::chrono::milliseconds{10L},
            std::chrono::milliseconds{12L},
            2,
            servers - 1,
            std::chrono::milliseconds{4L},
            std::chrono::milliseconds{4L}
        ),
        cb,
        ids,
        {},
        nullptr
    };
    handle->raft = std::make_unique<raft::RaftImpl<GoRPCClient>>(
        peers,
        selfId,
        *handle->serviceChannel,
        *handle->followerChannel,
        handle->policy,
        [handle, cb](std::string address, zdb::RetryPolicy p) -> GoRPCClient& {
            handle->clients[address] = std::make_unique<GoRPCClient>(handle->peerIds[address], address, p, cb);
            return *(handle->clients[address]);
        }
    );
    return handle;
}

int handle_request_vote(RaftHandle* h, char* args, int args_size, char* reply) {
    if (!h || !h->raft) {
        return 0;
    }
    raft::proto::RequestVoteArg protoArgs{};
    auto s = std::string{args, static_cast<size_t>(args_size)};
    if (!protoArgs.ParseFromString(s)) {
        return 0;
    }
    auto r = h->raft->requestVoteHandler(protoArgs);
    raft::proto::RequestVoteReply protoReply{};
    protoReply.set_term(r.term);
    protoReply.set_votegranted(r.voteGranted);
    std::string reply_str;
    if (!protoReply.SerializeToString(&reply_str)) {
        return 0;
    }
    memcpy(reply, reply_str.data(), reply_str.size());
    return reply_str.size();
}

int handle_append_entries(RaftHandle* h, char* args, int args_size, char* reply) {
    if (!h || !h->raft) {
        return 0;
    }
    raft::proto::AppendEntriesArg protoArgs{};
    auto s = std::string {args, static_cast<size_t>(args_size)};
    if (!protoArgs.ParseFromString(s)) {
        return 0;
    }
    auto r = h->raft->appendEntriesHandler(protoArgs);
    raft::proto::AppendEntriesReply protoReply{};
    protoReply.set_success(r.success);
    protoReply.set_term(r.term);
    std::string reply_str;
    if (!protoReply.SerializeToString(&reply_str)) {
        return 0;
    }
    memcpy(reply, reply_str.data(), reply_str.size());
    return reply_str.size();
}


int raft_get_state(RaftHandle* handle, int* term, int* is_leader) {
    if (!handle || !handle->raft) {
        return 0;
    }
    try {
        auto current_term = handle->raft->getCurrentTerm();
        auto role = handle->raft->getRole();

        *term = current_term;
        *is_leader = (role == raft::Role::Leader) ? 1 : 0;

        return 1;
    } catch (const std::exception& e) {
        // std::cerr << "Error getting state: " << e.what() << std::endl;
        return 0;
    }
}

}
