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
	"bytes"
	"fmt"
	"sync"
	"unsafe"

	"6.5840/kvsrv1/rpc"
	"6.5840/labgob"
	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
	proto_raft "github.com/as-family/zdb/proto"
	uuid "github.com/google/uuid"
	protobuf "google.golang.org/protobuf/proto"
)

var useRaftStateMachine bool // to plug in another raft besided raft1

type Op struct {
	// Your definitions here.
	// Field names must start with capital letters,
	// otherwise RPC will break.
	Req any
	Id  int64
	Rep any
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
	mu             sync.Mutex
	me             int
	rf             raftapi.Raft
	applyCh        chan raftapi.ApplyMsg
	maxraftstate   int // snapshot if log grows this big
	sm             StateMachine
	handle         *C.RsmHandle
	rpcCb          C.uintptr_t
	channelCb      C.uintptr_t
	recChannelCb   C.uintptr_t
	closeChannelCb C.uintptr_t
	persisterCb    C.uintptr_t
	smCb           C.uintptr_t
	dead           int32
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
		dead:         0,
	}
	rsm.rpcCb = registerCallback(servers, &rsm.dead)
	rsm.channelCb = channelRegisterCallback(rsm.applyCh, &rsm.dead)
	rsm.persisterCb = registerPersister(persister)
	fmt.Println("GO MakeRSM persister handle", rsm.persisterCb)
	rsm.smCb = registerStateMachine(&sm)
	rsm.recChannelCb = receiveChannelRegisterCallback(rsm.applyCh, &rsm.dead)
	rsm.closeChannelCb = registerChannel(rsm.applyCh)
	rsm.handle = C.create_rsm(C.int(rsm.me), C.int(len(servers)), rsm.rpcCb, rsm.channelCb, rsm.recChannelCb, rsm.closeChannelCb, rsm.persisterCb, C.int(maxraftstate), rsm.smCb)
	if rsm.handle == nil {
		fmt.Println("Go: Failed to create Raft node", me)
		GoFreeCallback(rsm.rpcCb)
		GoFreeCallback(rsm.channelCb)
		GoFreeCallback(rsm.persisterCb)
		GoFreeCallback(rsm.smCb)
		GoFreeCallback(rsm.recChannelCb)
		GoFreeCallback(rsm.closeChannelCb)
		return nil
	}
	rsm.rf = &Raft{
		handle:      C.rsm_raft_handle(rsm.handle),
		peers:       servers,
		me:          me,
		persister:   persister,
		applyCh:     rsm.applyCh,
		cb:          rsm.rpcCb,
		channelCb:   rsm.channelCb,
		persisterCB: rsm.persisterCb,
		dead:        0,
		rsmHandle:   rsm.handle,
	}
	return rsm
}

func (rsm *RSM) Raft() raftapi.Raft {
	return rsm.rf
}

func (rsm *RSM) Kill() {
	fmt.Println("GO RSM Kill")
	// close(rsm.applyCh)
	if rsm.handle == nil {
		return
	}
	rsm.handle = nil
	C.rsm_kill(rsm.handle)
	GoFreeCallback(rsm.rpcCb)
	GoFreeCallback(rsm.channelCb)
	GoFreeCallback(rsm.persisterCb)
	GoFreeCallback(rsm.smCb)
	GoFreeCallback(rsm.recChannelCb)
	GoFreeCallback(rsm.closeChannelCb)
}

// Submit a command to Raft, and wait for it to be committed.  It
// should return ErrWrongLeader if client should find new leader and
// try again.
func (rsm *RSM) Submit(req any) (rpc.Err, any) {

	// Submit creates an Op structure to run a command through Raft;
	// for example: op := Op{Me: rsm.me, Id: id, Req: req}, where req
	// is the argument to Submit and id is a unique id for the op.

	// your code here

	fmt.Println("Submit req =", req)
	op := Op{
		Req: req,
		Id:  0,
	}
	cmd := &proto_raft.Command{}
	cmd.Value = &proto_raft.Value{}
	cmd.Op = "r"
	u, erru := uuid.NewV7()
	if erru != nil {
		fmt.Println("GO Submit could not generate uuid", erru)
		return rpc.ErrWrongLeader, nil
	}
	ub, errb := u.MarshalBinary()
	if errb != nil {
		fmt.Println("GO Submit could not marshal uuid", errb)
		return rpc.ErrWrongLeader, nil
	}
	cmd.RequestID = &proto_raft.RequestID{
		Uuid: ub,
	}
	w := new(bytes.Buffer)
	e := labgob.NewEncoder(w)
	e.Encode(op)
	cmd.Value.Data = w.Bytes()
	b, err := protobuf.Marshal(cmd)
	if err != nil {
		fmt.Println("Go Submit: could not marshal cmd")
		return rpc.ErrWrongLeader, nil
	}
	replyBuf := make([]byte, 1024)
	size := int(C.rsm_submit(rsm.handle, unsafe.Pointer(&b[0]), C.int(len(b)), unsafe.Pointer(&replyBuf[0])))
	if size <= 0 {
		fmt.Println("Go Submit: rsm_submit returned size", size)
		// rsm.Kill()
		return rpc.ErrWrongLeader, nil
	}
	state := &proto_raft.State{}
	err = protobuf.Unmarshal(replyBuf[:size], state)
	if err != nil {
		fmt.Println("Go Submit: could not unmarshal state")
		return rpc.ErrWrongLeader, nil
	}
	switch x := state.Result.(type) {
	case *proto_raft.State_Error:
		return rpc.ErrWrongLeader, nil
	case *proto_raft.State_Value:
		y := Op{}
		w2 := bytes.NewBuffer(x.Value.Data)
		e2 := labgob.NewDecoder(w2)
		err = e2.Decode(&y)
		if err != nil {
			fmt.Println("Go Submit: could not decode state")
			return rpc.ErrWrongLeader, nil
		}
		return rpc.OK, y.Rep
	default:
		fmt.Println("Go Submit: could not switch on type")
		return rpc.ErrWrongLeader, nil
	}
}
