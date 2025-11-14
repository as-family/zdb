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
	v, ok := cbStore.Load(h)
	if !ok {
		return nil, false
	}
	return v, true
}

func deleteHandle(h uintptr) {
	cbStore.Delete(h)
}

func registerCallback(peers []*labrpc.ClientEnd, dead int32) C.uintptr_t {
	cb := callbackFn(func(p int, s string, a interface{}, b interface{}) int {
		if atomic.LoadInt32(&dead) == 1 {
			return 1
		}
		if v, ok := rafts.Load(p); !ok || !v.(bool) {
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

func channelRegisterCallback(applyCh chan raftapi.ApplyMsg, dead int32) C.uintptr_t {
	cb := goChannelCallbackFn(func(s []byte, i int) int {
		protoC := &proto_raft.Command{}
		err := protobuf.Unmarshal(s, protoC)
		if err != nil {
			fmt.Println("Error: failed to unmarshal Command in channel callback", err)
			return 1
		}
		if protoC.Op != "i" {
			var x interface{}
			if (protoC.Key == nil) || (protoC.Key.Data == "") {
				x = 0
			} else {
				x, err = strconv.Atoi(protoC.Key.Data)
				if err != nil {
					x = protoC.Key.Data
				}
			}
			if atomic.LoadInt32(&dead) == 1 {
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
				if atomic.LoadInt32(&dead) == 1 {
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
