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
#include "goRaft/cgo/raft_wrapper.hpp"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
*/
import "C"

import (
	"bytes"
	"context"
	"fmt"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"

	"6.5840/labgob"
	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
	proto_raft "github.com/as-family/zdb/proto"
	protobuf "google.golang.org/protobuf/proto"
)

type labRpcFn func(int, string, interface{}, interface{}) int

var (
	cbStore   sync.Map
	cbCounter uint64
)

func newHandle(cb interface{}) uintptr {
	h := atomic.AddUint64(&cbCounter, 1)
	cbStore.Store(uintptr(h), cb)
	return uintptr(h)
}

func getCallback(h uintptr) (interface{}, bool) {
	return cbStore.Load(h)
}

func deleteHandle(h uintptr) {
	cbStore.Delete(h)
}

func registerLabRpcCallback(peers []*labrpc.ClientEnd, dead *int32) C.uintptr_t {
	cb := labRpcFn(func(p int, s string, a interface{}, b interface{}) int {
		if atomic.LoadInt32(dead) == 1 {
			return 1
		}
		if peers[p].Call(s, a, b) {
			return 0
		}
		return 1
	})
	h := newHandle(cb)
	return C.uintptr_t(h)
}

func registerPersister(persister *tester.Persister) C.uintptr_t {
	h := newHandle(persister)
	return C.uintptr_t(h)
}

func registerStateMachine(machine *StateMachine) C.uintptr_t {
	h := newHandle(machine)
	return C.uintptr_t(h)
}

func registerChannel(applyCh chan raftapi.ApplyMsg) C.uintptr_t {
	h := newHandle(applyCh)
	return C.uintptr_t(h)
}

func labRpcCall(h C.uintptr_t, p int, s string, a interface{}, b interface{}) int {
	cb, ok := getCallback(uintptr(h))
	if !ok {
		fmt.Printf("GO labRpcCall: invalid handle %d\n", uintptr(h))
		return 1
	}
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	done := make(chan int, 1)
	go func() {
		done <- cb.(labRpcFn)(p, s, a, b)
	}()
	select {
	case res := <-done:
		return res
	case <-ctx.Done():
		return 1
	}
}

func freeCallback(h C.uintptr_t) {
	deleteHandle(uintptr(h))
}

//export SendToApplyCh
func SendToApplyCh(handle C.uintptr_t, s unsafe.Pointer, s_len C.int, index C.int) C.int {
	if s_len <= 0 {
		return C.int(1)
	}
	var applyCh chan raftapi.ApplyMsg
	if i, ok := getCallback(uintptr(handle)); !ok {
		return C.int(1)
	} else {
		applyCh = i.(chan raftapi.ApplyMsg)
	}
	b := C.GoBytes(s, s_len)
	protoC := &proto_raft.Command{}
	if err := protobuf.Unmarshal(b, protoC); err != nil {
		return C.int(1)
	}
	var msg raftapi.ApplyMsg
	switch protoC.Op {
	case "i":
		msg = raftapi.ApplyMsg{
			CommandValid:  false,
			SnapshotValid: true,
			Snapshot:      protoC.Value.Data,
			SnapshotIndex: int(protoC.Index),
			SnapshotTerm:  int(protoC.Value.Version),
		}
	case "r":
		msg = raftapi.ApplyMsg{
			CommandValid: true,
			Command:      b,
			CommandIndex: int(index),
		}
	default:
		var command any
		if x, err := strconv.Atoi(protoC.Key.Data); err != nil {
			command = protoC.Key.Data
		} else {
			command = x
		}
		msg = raftapi.ApplyMsg{
			CommandValid: true,
			Command:      command,
			CommandIndex: int(index),
		}
	}
	ctx, cancel := context.WithTimeout(context.Background(), 250*time.Millisecond)
	defer cancel()
	select {
	case <- ctx.Done():
		return C.int(1)
	case applyCh <- msg:
		return C.int(0)
	}
}

//export ReceiveFromApplyCh
func ReceiveFromApplyCh(handle C.uintptr_t, reply unsafe.Pointer) C.int {
	var applyCh chan raftapi.ApplyMsg
	if i, ok := getCallback(uintptr(handle)); !ok {
		return C.int(1)
	} else {
		applyCh = i.(chan raftapi.ApplyMsg)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancel()
	var msg raftapi.ApplyMsg
	select {
	case <-ctx.Done():
		return -1
	case m, ok := <- applyCh:
		if !ok {
			return -2
		} else {
			msg = m
		}
	}
	var b []byte
	if msg.CommandValid {
		switch cmd := msg.Command.(type) {
		case string:
			b = []byte(cmd)
		case int:
			b = []byte(strconv.Itoa(cmd))
		case []byte:
			b = cmd
		default:
			fmt.Printf("ReceiveFromApplyCh: unexpected command type %T\n", msg.Command)
			return -1
		}
	} else if msg.SnapshotValid {
		protoC := &proto_raft.Command {
			Index: uint64(msg.SnapshotIndex),
			Value: &proto_raft.Value {
				Data: msg.Snapshot,
				Version: uint64(msg.SnapshotTerm),
			},
		}
		if bs, err :=  protobuf.Marshal(protoC); err != nil {
			return -1
		} else {
			b = bs
		}
	} else {
		return -1
	}
	C.memmove(reply, unsafe.Pointer(&b[0]), C.size_t(len(b)))
	return C.int(len(b))
}

//export CloseApplyCh
func CloseApplyCh(handle C.uintptr_t) {
	c, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Printf("channel_close_callback: invalid handle %d\n", uintptr(handle))
		return
	}
	applyCh := c.(chan raftapi.ApplyMsg)
	close(applyCh)
}

//export persister_go_invoke_callback
func persister_go_invoke_callback(handle C.uintptr_t, data unsafe.Pointer, data_len C.int) C.int {
	ps, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Printf("persister_go_invoke_callback: invalid handle %d\n", uintptr(handle))
		return C.int(1)
	}
	p := ps.(*tester.Persister)
	protoState := &proto_raft.PersistentState{}
	err := protobuf.Unmarshal(C.GoBytes(data, data_len), protoState)
	if err != nil {
		fmt.Println("Error: failed to unmarshal PersistentState")
		return C.int(1)
	}
	state := PersistentState{
		CurrentTerm:       protoState.CurrentTerm,
		Log:               make(Log, len(protoState.Log)),
		SnapshotData:      protoState.Snapshot,
		LastIncludedIndex: protoState.LastIncludedIndex,
		LastIncludedTerm:  protoState.LastIncludedTerm,
	}
	if protoState.VotedFor != nil {
		state.VotedFor = *protoState.VotedFor
	}
	for i, entry := range protoState.Log {
		state.Log[i] = LogEntry{
			Index:   entry.Index,
			Term:    entry.Term,
			Command: string(entry.Command),
		}
	}
	w := new(bytes.Buffer)
	e := labgob.NewEncoder(w)
	e.Encode(state.CurrentTerm)
	e.Encode(state.VotedFor)
	e.Encode(state.Log)
	e.Encode(state.LastIncludedIndex)
	e.Encode(state.LastIncludedTerm)
	raftstate := w.Bytes()
	p.Save(raftstate, state.SnapshotData)
	return C.int(0)
}

//export persister_go_read_callback
func persister_go_read_callback(handle C.uintptr_t, out unsafe.Pointer, out_len C.int) C.int {
	ps, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Printf("persister_go_read_callback: invalid handle %d\n", uintptr(handle))
		return C.int(-1)
	}
	p := ps.(*tester.Persister)
	data := p.ReadRaftState()
	sd := p.ReadSnapshot()
	if len(data) < 1 {
		return C.int(-1)
	}
	r := bytes.NewBuffer(data)
	d := labgob.NewDecoder(r)
	protoState := &proto_raft.PersistentState{}
	state := PersistentState{}
	if d.Decode(&state.CurrentTerm) != nil ||
		d.Decode(&state.VotedFor) != nil ||
		d.Decode(&state.Log) != nil ||
		d.Decode(&state.LastIncludedIndex) != nil ||
		d.Decode(&state.LastIncludedTerm) != nil {
		fmt.Println("Error: failed to decode persisted state in persister_go_read_callback")
		return C.int(-1)
	} else {
		protoState.CurrentTerm = state.CurrentTerm
		protoState.VotedFor = &state.VotedFor
		protoState.LastIncludedIndex = state.LastIncludedIndex
		protoState.LastIncludedTerm = state.LastIncludedTerm
		protoState.Log = make([]*proto_raft.LogEntry, len(state.Log))
		for i, entry := range state.Log {
			protoState.Log[i] = &proto_raft.LogEntry{
				Index:   entry.Index,
				Term:    entry.Term,
				Command: []byte(entry.Command),
			}
		}
		protoState.Snapshot = sd
		stateBytes, err := protobuf.Marshal(protoState)
		if err != nil {
			fmt.Println("Error: failed to marshal protoState in persister_go_read_callback")
			return C.int(-1)
		}
		if len(stateBytes) > int(out_len) {
			fmt.Println("Error: buffer too small in persister_go_read_callback")
			return C.int(-1)
		}
		C.memmove(out, unsafe.Pointer(&stateBytes[0]), C.size_t(len(stateBytes)))
		return C.int(len(stateBytes))
	}
}

//export state_machine_go_apply_command
func state_machine_go_apply_command(handle C.uintptr_t, command unsafe.Pointer, command_len C.int, out unsafe.Pointer) C.int {
	m, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Println("GO state_machine_go_apply_command: bad handle ", handle)
		return -1
	}
	machine := *m.(*StateMachine)
	protoC := &proto_raft.Command{}
	if err := protobuf.Unmarshal(C.GoBytes(command, command_len), protoC); err != nil {
		fmt.Println("GO state_machine_go_apply_command: could not unmarshal command")
		return -1
	}
	if protoC.Value == nil {
		fmt.Println("GO state_machine_go_apply_command: nil Value")
		return -1
	}
	if len(protoC.Value.Data) < 1 {
		fmt.Println("GO state_machine_go_apply_command: No DATA")
		return -1
	}
	w2 := bytes.NewBuffer(protoC.Value.Data)
	e2 := labgob.NewDecoder(w2)
	x := Op{}
	err := e2.Decode(&x)
	if err != nil {
		fmt.Println("GO state_machine_go_apply_command could not decode ", err)
		return -1
	}
	state := machine.DoOp(x.Req)
	w := new(bytes.Buffer)
	e := labgob.NewEncoder(w)
	stateOp := Op{
		Req: nil,
		Rep: state,
		Id:  0,
	}
	e.Encode(stateOp)
	protoState := &proto_raft.State{
		Result: &proto_raft.State_Value{Value: &proto_raft.Value{Data: w.Bytes()}},
	}
	b, err := protobuf.Marshal(protoState)
	if err != nil {
		fmt.Println("GO state_machine_go_apply_command: could not marshal protoState")
		return -1
	}
	C.memmove(out, unsafe.Pointer(&b[0]), C.size_t(len(b)))
	return C.int(len(b))
}
