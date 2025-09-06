#ifndef RAFT_WRAPPER_H
#define RAFT_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RaftHandle RaftHandle;
RaftHandle* create_raft(int id, int servers, uintptr_t handle);
void kill_raft(RaftHandle* h);
int handle_request_vote(RaftHandle* h, char* args, int args_size, char* reply);
int handle_append_entries(RaftHandle* h, char* args, int args_size, char* reply);
int raft_get_state(RaftHandle* handle, int* term, int* is_leader);

#ifdef __cplusplus
}
#endif

#endif // RAFT_WRAPPER_H
