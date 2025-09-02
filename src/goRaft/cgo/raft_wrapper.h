#ifndef RAFT_WRAPPER_H
#define RAFT_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RaftHandle RaftHandle;

// Lifecycle functions
RaftHandle* raft_create(char** servers, int num_servers, int me, char* persister_id);
void raft_connect_all_peers(RaftHandle* handle);
void raft_destroy(RaftHandle* handle);
void raft_kill(RaftHandle* handle);

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
