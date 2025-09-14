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
#cgo CPPFLAGS: -I${SRCDIR}/../..
#cgo LDFLAGS: -L${SRCDIR}/../../out/build/gcc-14/lib -lzdb_raft
#include "cgo/raft_wrapper.hpp"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Forward declaration of the Go function C++ will call.
extern int go_invoke_callback(uintptr_t handle, int, char*, void*, int, void*, int);
extern int channel_go_invoke_callback(uintptr_t handle, void*, int, int);

// C wrapper that C++ will call (calls the exported Go function).
// We declare it here so the C++ library can link against it.
int go_invoke_callback(uintptr_t handle, int v, char*, void*, int, void*, int);
int channel_go_invoke_callback(uintptr_t handle, void*, int, int);
*/
import "C"

import (
	"fmt"
	"strconv"
	"sync"
	"sync/atomic"
	"unsafe"

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

func newHandle(cb callbackFn) uintptr {
	h := atomic.AddUint64(&cbCounter, 1)
	cbStore.Store(uintptr(h), cb)
	return uintptr(h)
}

func getCallback(h uintptr) (callbackFn, bool) {
	v, ok := cbStore.Load(h)
	if !ok {
		return nil, false
	}
	return v.(callbackFn), true
}

func deleteHandle(h uintptr) {
	cbStore.Delete(h)
}

func registerCallback(rf *Raft) C.uintptr_t {
	cb := func(p int, s string, a interface{}, b interface{}) int {
		if rf.peers[p].Call(s, a, b) {
			return 1
		}
		return 0
	}
	h := newHandle(cb)
	return C.uintptr_t(h)
}

//export GoInvokeCallback
func GoInvokeCallback(h C.uintptr_t, p int, s string, a interface{}, b interface{}) int {
	cb, ok := getCallback(uintptr(h))
	if !ok {
		fmt.Printf("Go: invalid handle %d\n", uintptr(h))
		return 0
	}
	return cb(p, s, a, b)
}

func GoFreeCallback(h C.uintptr_t) {
	deleteHandle(uintptr(h))
}

type goChannelCallbackFn func(string, int) int

var (
	channelCbStore   sync.Map
	channelCbCounter uint64
)

func channelNewHandle(cb goChannelCallbackFn) uintptr {
	h := atomic.AddUint64(&channelCbCounter, 1)
	channelCbStore.Store(uintptr(h), cb)
	return uintptr(h)
}

func channelGetCallback(h uintptr) (goChannelCallbackFn, bool) {
	v, ok := channelCbStore.Load(h)
	if !ok {
		return nil, false
	}
	return v.(goChannelCallbackFn), true
}

func channelDeleteHandle(h uintptr) {
	channelCbStore.Delete(h)
}

func channelRegisterCallback(rf *Raft) C.uintptr_t {
	cb := func(s string, i int) int {
		protoC := &proto_raft.Command{}
        err := protobuf.Unmarshal([]byte(s), protoC)
        if err != nil {
//             panic("could not unmarshal Command")
        }
        var x interface{}
        x, err = strconv.Atoi(protoC.Op)
        if err != nil {
            x = protoC.Op
        }
//         fmt.Println("Go: channel callback invoked:", x)
		if rf.applyCh != nil {
			rf.applyCh <- raftapi.ApplyMsg{
				CommandValid: true,
				Command:      x,
				CommandIndex: i,
			}
//             fmt.Println("Go: command sent to applyCh:", x)
			return 1
		}
		return 0
	}
	h := channelNewHandle(cb)
	return C.uintptr_t(h)
}

//export ChannelGoInvokeCallback
func ChannelGoInvokeCallback(h C.uintptr_t, s string, i int) int {
	cb, ok := channelGetCallback(uintptr(h))
	if !ok {
		fmt.Printf("Go: invalid handle %d\n", uintptr(h))
		return 0
	}
	return cb(s, i)
}

func ChannelGoFreeCallback(h C.uintptr_t) {
	channelDeleteHandle(uintptr(h))
}

func go_invoke_request_vote(handle C.uintptr_t, p C.int, s string, args unsafe.Pointer, args_len C.int, reply unsafe.Pointer, reply_len C.int) C.int {
	protoArg := &proto_raft.RequestVoteArg{}
	err := protobuf.Unmarshal(C.GoBytes(args, args_len), protoArg)
	if err != nil {
		fmt.Println("Error: failed to unmarshal RequestVoteArg")
		return C.int(-1)
	}
	arg := &RequestVoteArg{
		CandidateId:  protoArg.CandidateId,
		Term:         protoArg.Term,
		LastLogIndex: protoArg.LastLogIndex,
		LastLogTerm:  protoArg.LastLogTerm,
	}
	rep := &RequestVoteReply{}
	result := GoInvokeCallback(handle, int(p), s, arg, rep)
	if result == 0 {
		return C.int(-1)
	}
	protoReply := &proto_raft.RequestVoteReply{
		VoteGranted: rep.VoteGranted,
		Term:        rep.Term,
	}
	replyBytes, err := protobuf.Marshal(protoReply)
	if err != nil {
		fmt.Println("Error: failed to marshal")
		return C.int(-1)
	}
	if len(replyBytes) != 0 {
		if len(replyBytes) > int(reply_len) {
			fmt.Println("Error: reply buffer too small for RequestVoteReply")
			return C.int(-1)
		}
		C.memmove(reply, unsafe.Pointer(&replyBytes[0]), C.size_t(len(replyBytes)))
	}
	return C.int(len(replyBytes))
}

func go_invoke_append_entries(handle C.uintptr_t, p C.int, s string, args unsafe.Pointer, args_len C.int, reply unsafe.Pointer, reply_len C.int) C.int {
	protoArg := &proto_raft.AppendEntriesArg{}
	err := protobuf.Unmarshal(C.GoBytes(args, args_len), protoArg)
	if err != nil {
		fmt.Println("Error: failed to unmarshal AppendEntriesArg")
		return C.int(-1)
	}
	entries := make(Log, len(protoArg.Entries))
	for i, entry := range protoArg.Entries {
		entries[i] = LogEntry{
			Index:   entry.Index,
			Term:    entry.Term,
			Command: string(entry.Command),
		}
	}
	arg := &AppendEntriesArg{
		LeaderId:     protoArg.LeaderId,
		Term:         protoArg.Term,
		PrevLogIndex: protoArg.PrevLogIndex,
		PrevLogTerm:  protoArg.PrevLogTerm,
		LeaderCommit: protoArg.LeaderCommit,
		Entries:      entries,
	}
	rep := &AppendEntriesReply{}
	result := GoInvokeCallback(handle, int(p), s, arg, rep)
	if result == 0 {
		return C.int(-1)
	}
	protoReply := &proto_raft.AppendEntriesReply{
		Success: rep.Success,
		Term:    rep.Term,
	}
	replyBytes, err := protobuf.Marshal(protoReply)
	if err != nil {
		fmt.Println("Error: failed to marshal")
		return C.int(-1)
	}
	if len(replyBytes) != 0 {
		if len(replyBytes) > int(reply_len) {
			fmt.Println("Error: reply buffer too small for AppendEntriesReply")
			return C.int(-1)
		}
		C.memmove(reply, unsafe.Pointer(&replyBytes[0]), C.size_t(len(replyBytes)))
	}
	return C.int(len(replyBytes))
}

//export go_invoke_callback
func go_invoke_callback(handle C.uintptr_t, p C.int, s *C.char, args unsafe.Pointer, args_len C.int, reply unsafe.Pointer, reply_len C.int) C.int {
	f := C.GoString(s)
	if f == "Raft.RequestVote" {
		return go_invoke_request_vote(handle, p, f, args, args_len, reply, reply_len)
	} else if f == "Raft.AppendEntries" {
		return go_invoke_append_entries(handle, p, f, args, args_len, reply, reply_len)
	} else {
		fmt.Printf("Go: unknown callback function: %s\n", f)
		return C.int(-1)
	}
}

//export channel_go_invoke_callback
func channel_go_invoke_callback(handle C.uintptr_t, s unsafe.Pointer, s_len C.int, i C.int) C.int {
	if s == nil || s_len <= 0 {
		return C.int(0)
	}
	str := C.GoStringN((*C.char)(s), s_len)
	return C.int(ChannelGoInvokeCallback(handle, str, int(i)))
}

type Raft struct {
	handle     *C.RaftHandle
	mu         sync.Mutex
	peers      []*labrpc.ClientEnd
	me         int
	dead       int32
	persister  *tester.Persister
	applyCh    chan raftapi.ApplyMsg
	cb         C.uintptr_t
	channelCb  C.uintptr_t
}

type LogEntry struct {
	Index   uint64
	Term    uint64
	Command string
}

type Log []LogEntry

type AppendEntriesArg struct {
	LeaderId     string
	Term         uint64
	PrevLogIndex uint64
	PrevLogTerm  uint64
	LeaderCommit uint64
	Entries      Log
}

type AppendEntriesReply struct {
	Success bool
	Term    uint64
}

type RequestVoteArg struct {
	CandidateId  string
	Term         uint64
	LastLogIndex uint64
	LastLogTerm  uint64
}

type RequestVoteReply struct {
	VoteGranted bool
	Term        uint64
}

func (rf *Raft) GetState() (int, bool) {
	if rf.handle == nil {
		return 0, false
	}
	var term C.int
	var isleader C.int
	if C.raft_get_state(rf.handle, &term, &isleader) == 0 {
		panic("Could not get state")
	}
	return int(term), isleader == 1
}

func (rf *Raft) Start(command interface{}) (int, int, bool) {
	if rf.handle == nil {
		return 0, 0, false
	}

	// Convert command to string (similar to how it's handled in other parts of the code)
	var commandStr string
	if cmd, ok := command.(string); ok {
		commandStr = cmd
	} else {
		// Convert to string representation if not already a string
		commandStr = fmt.Sprintf("%v", command)
	}
	index := 0
	term := 0
	isLeader := true

	var commandPtr unsafe.Pointer
	var commandSize int
    commandBytes := []byte(commandStr)
	if len(commandBytes) > 0 {
		commandPtr = unsafe.Pointer(&commandBytes[0])
		commandSize = len(commandBytes)
	} else {
		commandPtr = nil
		commandSize = 0
	}

	r := C.raft_start(rf.handle, commandPtr, C.int(commandSize), (*C.int)(unsafe.Pointer(&index)), (*C.int)(unsafe.Pointer(&term)), (*C.int)(unsafe.Pointer(&isLeader)))
	if r == 0 {
		// Could not start
		return -1, -1, false
	}
	return index, term, isLeader
}

func (rf *Raft) Kill() {
	if rf.handle == nil {
		// fmt.Println("Go: Raft handle is nil, already killed", rf.me)
		return
	}
	// fmt.Println("Go: Raft is being killed", rf.me)

	// Set handle to nil first to prevent double-kill
	handle := rf.handle
	rf.handle = nil

	C.kill_raft(handle)
	GoFreeCallback(rf.cb)
	ChannelGoFreeCallback(rf.channelCb)
	// fmt.Println("Go: Raft killed", rf.me)
}

func (rf *Raft) Snapshot(index int, snapshot []byte) {
	// Your code here, if desired.
}

func (rf *Raft) PersistBytes() int {
	return rf.persister.RaftStateSize()
}

func (rf *Raft) RequestVote(args *RequestVoteArg, reply *RequestVoteReply) {
	if rf.handle == nil {
		return
	}

	// Prepare the protobuf request
	protoArgs := &proto_raft.RequestVoteArg{
		CandidateId:  args.CandidateId,
		Term:         args.Term,
		LastLogIndex: args.LastLogIndex,
		LastLogTerm:  args.LastLogTerm,
	}
	strArgs, err := protobuf.Marshal(protoArgs)
	if err != nil || len(strArgs) <= 0 {
		fmt.Println("Error: failed to marshal RequestVoteArg")
		return
	}

	replyBuf := make([]byte, 1024)
	n := C.handle_request_vote(rf.handle,
		(*C.char)(unsafe.Pointer(&strArgs[0])), C.int(len(strArgs)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])))

	// Process the response
	if int(n) <= 0 || int(n) > len(replyBuf) {
		fmt.Println("Error: invalid reply size for RequestVote")
		return
	}
	protoReply := &proto_raft.RequestVoteReply{}
	err = protobuf.Unmarshal(replyBuf[:n], protoReply)
	if err != nil {
		fmt.Println("Error: failed to unmarshal RequestVoteReply")
		return
	}
	reply.Term = protoReply.Term
	reply.VoteGranted = protoReply.VoteGranted
}

func (rf *Raft) AppendEntries(args *AppendEntriesArg, reply *AppendEntriesReply) {
	if rf.handle == nil {
		return
	}

	// Prepare the protobuf request
	protoEntries := make([]*proto_raft.LogEntry, len(args.Entries))
	for i, entry := range args.Entries {
		protoEntries[i] = &proto_raft.LogEntry{
			Index:   entry.Index,
			Term:    entry.Term,
			Command: []byte(entry.Command),
		}
	}
	protoArgs := &proto_raft.AppendEntriesArg{
		LeaderId:     args.LeaderId,
		Term:         args.Term,
		PrevLogIndex: args.PrevLogIndex,
		PrevLogTerm:  args.PrevLogTerm,
		LeaderCommit: args.LeaderCommit,
		Entries:      protoEntries,
	}
	strArgs, err := protobuf.Marshal(protoArgs)
	if err != nil {
		fmt.Println("Error: failed to marshal AppendEntriesArg")
		return
	}

	// Call C++ without holding any Go locks - C++ should handle its own synchronization
	replyBuf := make([]byte, 1024)
	n := C.handle_append_entries(rf.handle,
		(*C.char)(unsafe.Pointer(&strArgs[0])), C.int(len(strArgs)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])))
	if n <= 0 || int(n) > len(replyBuf) {
		fmt.Println("Error: invalid reply size for AppendEntries")
		return
	}
	// Process the response
	protoReply := &proto_raft.AppendEntriesReply{}
	err = protobuf.Unmarshal(replyBuf[:n], protoReply)
	if err != nil {
		fmt.Println("Error: failed to unmarshal AppendEntriesReply")
		return
	}
	reply.Success = protoReply.Success
	reply.Term = protoReply.Term
}

func Make(peers []*labrpc.ClientEnd, me int, persister *tester.Persister, applyCh chan raftapi.ApplyMsg) raftapi.Raft {
	rf := &Raft{}
	rf.peers = peers
	rf.me = me
	rf.persister = persister
	rf.applyCh = applyCh
	rf.cb = registerCallback(rf)
	rf.channelCb = channelRegisterCallback(rf)
	rf.handle = C.create_raft(C.int(me), C.int(len(peers)), C.uintptr_t(rf.cb), C.uintptr_t(rf.channelCb))
	if rf.handle == nil {
		// Avoid returning a half-initialized Raft
		GoFreeCallback(rf.cb)
		return nil
	}
	return rf
}
