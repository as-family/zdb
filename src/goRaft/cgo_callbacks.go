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

// C wrapper that C++ will call (calls the exported Go function).
// We declare it here so the C++ library can link against it.
int go_invoke_callback(uintptr_t handle, int v, char*, void*, int, void*, int);
int channel_go_invoke_callback(uintptr_t handle, void*, int, int);
int persister_go_read_callback(uintptr_t handle, void*, int);
int state_machine_go_apply_command(uintptr_t, void*, int, void*);
*/
import "C"

import (
	"bytes"
	"context"
	"errors"
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

type callbackFn func(int, string, interface{}, interface{}) int

var (
	cbStore   sync.Map
	cbCounter uint64
)

var rafts sync.Map

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

func registerCallback(peers []*labrpc.ClientEnd, dead *int32) C.uintptr_t {
	cb := callbackFn(func(p int, s string, a interface{}, b interface{}) int {
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

//export GoInvokeCallback
func GoInvokeCallback(h C.uintptr_t, p int, s string, a interface{}, b interface{}) int {
	cb, ok := getCallback(uintptr(h))
	if !ok {
		fmt.Printf("GoInvokeCallback: invalid handle %d\n", uintptr(h))
		return 1
	}
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	done := make(chan int, 1)
	go func() {
		done <- cb.(callbackFn)(p, s, a, b)
	}()
	select {
	case res := <-done:
		return res
	case <-ctx.Done():
		return 1
	}
}

func GoFreeCallback(h C.uintptr_t) {
	deleteHandle(uintptr(h))
}

type goChannelCallbackFn func([]byte, int) int

func channelRegisterCallback(applyCh chan raftapi.ApplyMsg, dead *int32) C.uintptr_t {
	cb := goChannelCallbackFn(func(s []byte, i int) int {
		protoC := &proto_raft.Command{}
		err := protobuf.Unmarshal(s, protoC)
		if err != nil {
			fmt.Println("Error: failed to unmarshal Command in channel callback", err)
			return 1
		}
		if protoC.Op != "i" {
			if protoC.Op == "r" {
				if atomic.LoadInt32(dead) == 1 {
					return 1
				}
				if applyCh != nil {
					applyCh <- raftapi.ApplyMsg{
						CommandValid: true,
						Command:      s,
						CommandIndex: i,
					}
					return 0
				}
			}
			var x interface{}
			if (protoC.Key == nil) || (protoC.Key.Data == "") {
				x = 0
			} else {
				x, err = strconv.Atoi(protoC.Key.Data)
				if err != nil {
					x = protoC.Key.Data
				}
			}
			if atomic.LoadInt32(dead) == 1 {
				return 1
			}
			if applyCh != nil {
				applyCh <- raftapi.ApplyMsg{
					CommandValid: true,
					Command:      x,
					CommandIndex: i,
				}
				return 0
			}
		} else {
			if applyCh != nil {
				ctx, cancel := context.WithTimeout(context.Background(), 250*time.Millisecond)
				defer cancel()
				if atomic.LoadInt32(dead) == 1 {
					return 1
				}
				select {
				case <-ctx.Done():
					return 1
				case applyCh <- raftapi.ApplyMsg{
					CommandValid:  false,
					SnapshotValid: true,
					Snapshot:      protoC.Value.Data,
					SnapshotIndex: int(protoC.Index),
					SnapshotTerm:  int(protoC.Value.Version),
				}:
					return 0
				}
			}
		}
		return 1
	})
	h := newHandle(cb)
	return C.uintptr_t(h)
}

func receiveChannelRegisterCallback(applyCh chan raftapi.ApplyMsg, dead *int32) C.uintptr_t {
	cb := func() (unsafe.Pointer, C.int, error) {
		if atomic.LoadInt32(dead) == 1 {
			return nil, 0, errors.New("died")
		}
		ctx, cancel := context.WithTimeout(context.Background(), 1*time.Millisecond)
		defer cancel()
		select {
		case <-ctx.Done():
			return nil, -1, errors.New("timeout")
		case msg, ok := <-applyCh:
			if !ok {
				return nil, -2, errors.New("channel closed")
			}
			if msg.CommandValid {
				var s []byte
				switch cmd := msg.Command.(type) {
				case string:
					s = []byte(cmd)
				case int:
					s = []byte(strconv.Itoa(cmd))
				case []byte:
					s = cmd
				}
				// protoC := &proto_raft.Command{
				// 	Index: uint64(msg.CommandIndex),
				// 	Value: &proto_raft.Value{Data: s},
				// 	Op:    "r",
				// }
				// b, err := protobuf.Marshal(protoC)
				// if err != nil {
				// return nil, 0, errors.New("bad command could not marshal")
				// }
				return unsafe.Pointer(&s[0]), C.int(len(s)), nil
			} else if msg.SnapshotValid {
				return nil, -1, errors.New("not IMplemented")
			} else {
				return nil, -1, errors.New("bad msg")
			}
		}
	}
	h := newHandle(cb)
	return C.uintptr_t(h)
}

//export receive_channel_go_callback
func receive_channel_go_callback(handle C.uintptr_t, reply unsafe.Pointer) C.int {
	cb, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Printf("receive_channel_go_callback: invalid handle %d\n", uintptr(handle))
		return -1
	}
	p, s, err := cb.(func() (unsafe.Pointer, C.int, error))()
	if err != nil {
		return C.int(s)
	}
	C.memmove(reply, p, C.size_t(s))
	return C.int(s)
}

//export channel_go_invoke_callback
func channel_go_invoke_callback(handle C.uintptr_t, s unsafe.Pointer, s_len C.int, i C.int) C.int {
	if s == nil || s_len <= 0 {
		return C.int(1)
	}
	bytes := C.GoBytes(s, s_len)
	cb, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Printf("channel_go_invoke_callback: invalid handle %d\n", uintptr(handle))
		return 1
	}
	return C.int(cb.(goChannelCallbackFn)(bytes, int(i)))
}

//export channel_is_closed_callback
func channel_is_closed_callback(handle C.uintptr_t) C.int {
	c, ok := getCallback(uintptr(handle))
	if !ok {
		fmt.Printf("channel_is_closed_callback: invalid handle %d\n", uintptr(handle))
		return C.int(0)
	}
	applyCh := c.(chan raftapi.ApplyMsg)
	ctx, cancel := context.WithTimeout(context.Background(), 1*time.Millisecond)
	defer cancel()
	select {
	case <-ctx.Done():
		return C.int(0)
	case v, open := <-applyCh:
		if !open {
			return C.int(1)
		}
		applyCh <- v
		return C.int(0)
	}
}

//export channel_close_callback
func channel_close_callback(handle C.uintptr_t) {
	fmt.Println("channel_close_callback")
	defer fmt.Println("channel_close_callback done")
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
	protobuf.Unmarshal(C.GoBytes(command, command_len), protoC)
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
