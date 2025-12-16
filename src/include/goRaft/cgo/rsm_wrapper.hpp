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

#ifndef RSM_WRAPPER_H
#define RSM_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RsmHandle RsmHandle;
typedef struct RaftHandle RaftHandle;

RsmHandle* create_rsm(int id, int servers, uintptr_t rpc, uintptr_t channel, uintptr_t persister, int maxraftstate, uintptr_t sm);
int rsm_submit(RsmHandle* handle, void* command, int command_size, void* state);
RaftHandle* rsm_raft_handle(RsmHandle* handle);
void rsm_kill(RsmHandle* handle);

#ifdef __cplusplus
}
#endif

#endif // RSM_WRAPPER_H
