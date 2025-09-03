#ifndef RAFT_WRAPPER_H
#define RAFT_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RaftHandle RaftHandle;

// Callback function type for labrpc calls
typedef int (*labrpc_call_func)(int caller_id, int peer_id, const char* service_method, 
                               const void* args, int args_size,
                               void* reply, int reply_size);

// Lifecycle functions
RaftHandle* raft_create(char** servers, int num_servers, int me, char* persister_id);
void raft_set_labrpc_callback(labrpc_call_func func);
void raft_destroy(RaftHandle* handle);
void raft_kill(RaftHandle* handle);

// RPC handlers (called by Go labrpc framework)
int raft_request_vote_handler(RaftHandle* handle, const char* args_data, int args_size,
                             char* reply_data, int reply_size);
int raft_append_entries_handler(RaftHandle* handle, const char* args_data, int args_size,
                               char* reply_data, int reply_size);

// Core Raft interface functions
int raft_start(RaftHandle* handle, char* command, int* index, int* term, int* is_leader);
int raft_get_state(RaftHandle* handle, int* term, int* is_leader);
int raft_persist_bytes(RaftHandle* handle);
void raft_snapshot(RaftHandle* handle, int index, char* snapshot, int snapshot_len);

// Channel functions for apply messages
typedef struct {
    int command_valid;
    char* command;
    int command_index;
    int snapshot_valid;
    char* snapshot;
    int snapshot_term;
    int snapshot_index;
} ApplyMsg;

int raft_receive_apply_msg(RaftHandle* handle, ApplyMsg* msg, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // RAFT_WRAPPER_H
