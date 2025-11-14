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

package zdb

/*
#cgo CXXFLAGS: -std=c++23
#cgo CPPFLAGS: -I${SRCDIR}/../include
#cgo LDFLAGS: -L${SRCDIR}/../../out/build/sys-gcc/lib -lzdb_raft
#include "goRaft/cgo/rsm_wrapper.hpp"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
*/
import "C"

import (
	"fmt"
	"sync"

	"6.5840/kvsrv1/rpc"
	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
)

var useRaftStateMachine bool // to plug in another raft besided raft1

type Op struct {
	// Your definitions here.
	// Field names must start with capital letters,
	// otherwise RPC will break.
}

// A server (i.e., ../server.go) that wants to replicate itself calls
// MakeRSM and must implement the StateMachine interface.  This
// interface allows the rsm package to interact with the server for
// server-specific operations: the server must implement DoOp to
// execute an operation (e.g., a Get or Put request), and
// Snapshot/Restore to snapshot and restore the server's state.
type StateMachine interface {
	DoOp(any) any
	Snapshot() []byte
	Restore([]byte)
}

type RSM struct {
	mu           sync.Mutex
	me           int
	rf           raftapi.Raft
	applyCh      chan raftapi.ApplyMsg
	maxraftstate int // snapshot if log grows this big
	sm           StateMachine
	handle       *C.RsmHandle
	rpcCb        C.uintptr_t
	channelCb    C.uintptr_t
	persisterCb  C.uintptr_t
	dead         int32
	// Your definitions here.
}

// servers[] contains the ports of the set of
// servers that will cooperate via Raft to
// form the fault-tolerant key/value service.
//
// me is the index of the current server in servers[].
//
// the k/v server should store snapshots through the underlying Raft
// implementation, which should call persister.SaveStateAndSnapshot() to
// atomically save the Raft state along with the snapshot.
// The RSM should snapshot when Raft's saved state exceeds maxraftstate bytes,
// in order to allow Raft to garbage-collect its log. if maxraftstate is -1,
// you don't need to snapshot.
//
// MakeRSM() must return quickly, so it should start goroutines for
// any long-running work.
func MakeRSM(servers []*labrpc.ClientEnd, me int, persister *tester.Persister, maxraftstate int, sm StateMachine) *RSM {
	rsm := &RSM{
		me:           me,
		maxraftstate: maxraftstate,
		applyCh:      make(chan raftapi.ApplyMsg),
		sm:           sm,
	}
	if !useRaftStateMachine {
		rsm.rf = Make(servers, me, persister, rsm.applyCh)
	}
	rsm.rpcCb = registerCallback(servers, rsm.dead)
	rsm.channelCb = channelRegisterCallback(rsm.applyCh, rsm.dead)
	rsm.persisterCb = registerPersister(persister)
	rsm.handle = C.create_rsm(C.int(rsm.me), C.int(len(servers)), C.uintptr_t(rsm.rpcCb), C.uintptr_t(rsm.channelCb), C.uintptr_t(rsm.persisterCb), C.int(maxraftstate), sm)
	if rsm.handle == nil {
		fmt.Println("Go: Failed to create Raft node", me)
		GoFreeCallback(rsm.rpcCb)
		GoFreeCallback(rsm.channelCb)
		GoFreeCallback(rsm.persisterCb)
		return nil
	}
	return rsm
}

func (rsm *RSM) Raft() raftapi.Raft {
	return rsm.rf
}

// Submit a command to Raft, and wait for it to be committed.  It
// should return ErrWrongLeader if client should find new leader and
// try again.
func (rsm *RSM) Submit(req any) (rpc.Err, any) {

	// Submit creates an Op structure to run a command through Raft;
	// for example: op := Op{Me: rsm.me, Id: id, Req: req}, where req
	// is the argument to Submit and id is a unique id for the op.

	// your code here
	return rpc.ErrWrongLeader, nil // i'm dead, try another server.
}
