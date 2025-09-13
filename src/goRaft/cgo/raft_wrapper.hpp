// SPDX-License-Identifier: AGPL-3.0-or-later
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
#ifndef RAFT_WRAPPER_H
#define RAFT_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RaftHandle RaftHandle;
RaftHandle* create_raft(int id, int servers, uintptr_t handle, uintptr_t channelCb);
void kill_raft(RaftHandle* h);
int handle_request_vote(RaftHandle* h, char* args, int args_size, char* reply);
int handle_append_entries(RaftHandle* h, char* args, int args_size, char* reply);
int raft_get_state(RaftHandle* handle, int* term, int* is_leader);
int raft_start(RaftHandle* handle, void* command, int command_size, int* index, int* term, int* is_leader);

#ifdef __cplusplus
}
#endif

#endif // RAFT_WRAPPER_H
