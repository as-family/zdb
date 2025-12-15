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
#include "goRaft/cgo/rsm_wrapper.hpp"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// C wrapper that C++ will call (calls the exported Go function).
// We declare it here so the C++ library can link against it.
int go_invoke_callback(uintptr_t handle, int v, char*, void*, int, void*, int);
int SendToApplyCh(uintptr_t handle, void*, int, int);
int persister_go_read_callback(uintptr_t handle, void*, int);
*/
import "C"

import (
	"fmt"
	"sync"
	"sync/atomic"
	"unsafe"

	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
	proto_raft "github.com/as-family/zdb/proto"
	protobuf "google.golang.org/protobuf/proto"
)

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
	if labRpcCall(handle, int(p), s, arg, rep) != 0 {
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
	if labRpcCall(handle, int(p), s, arg, rep) != 0 {
		return C.int(-1)
	}
	protoReply := &proto_raft.AppendEntriesReply{
		Success:       rep.Success,
		Term:          rep.Term,
		ConflictIndex: rep.ConflictIndex,
		ConflictTerm:  rep.ConflictTerm,
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

func go_invoke_install_snapshot(handle C.uintptr_t, p C.int, s string, args unsafe.Pointer, args_len C.int, reply unsafe.Pointer, reply_len C.int) C.int {
	protoArg := &proto_raft.InstallSnapshotArg{}
	err := protobuf.Unmarshal(C.GoBytes(args, args_len), protoArg)
	if err != nil {
		fmt.Println("Error: failed to unmarshal InstallSnapshotArg")
		return C.int(-1)
	}
	arg := &InstallSnapshotArg{
		LeaderId:          protoArg.LeaderId,
		Term:              protoArg.Term,
		LastIncludedIndex: protoArg.LastIncludedIndex,
		LastIncludedTerm:  protoArg.LastIncludedTerm,
		Data:              protoArg.Data,
	}
	rep := &InstallSnapshotReply{}
	if labRpcCall(handle, int(p), s, arg, rep) != 0 {
		return C.int(-1)
	}
	protoReply := &proto_raft.InstallSnapshotReply{
		Term:    rep.Term,
		Success: rep.Success,
	}
	replyBytes, err := protobuf.Marshal(protoReply)
	if err != nil {
		fmt.Println("Error: failed to marshal")
		return C.int(-1)
	}
	if len(replyBytes) != 0 {
		if len(replyBytes) > int(reply_len) {
			fmt.Println("Error: reply buffer too small for InstallSnapshotReply")
			return C.int(-1)
		}
		C.memmove(reply, unsafe.Pointer(&replyBytes[0]), C.size_t(len(replyBytes)))
	}
	return C.int(len(replyBytes))
}

//export go_invoke_callback
func go_invoke_callback(handle C.uintptr_t, p C.int, s *C.char, args unsafe.Pointer, args_len C.int, reply unsafe.Pointer, reply_len C.int) C.int {
	f := C.GoString(s)
	switch f {
	case "Raft.RequestVote":
		return go_invoke_request_vote(handle, p, f, args, args_len, reply, reply_len)
	case "Raft.AppendEntries":
		return go_invoke_append_entries(handle, p, f, args, args_len, reply, reply_len)
	case "Raft.InstallSnapshot":
		return go_invoke_install_snapshot(handle, p, f, args, args_len, reply, reply_len)
	default:
		fmt.Printf("Go: unknown callback function: %s\n", f)
		return C.int(-1)
	}
}

type Raft struct {
	handle         *C.RaftHandle
	mu             sync.Mutex
	peers          []*labrpc.ClientEnd
	me             int
	dead           int32
	persister      *tester.Persister
	applyCh        chan raftapi.ApplyMsg
	cb             C.uintptr_t
	channelCb      C.uintptr_t
	recChannelCb   C.uintptr_t
	closeChannelCb C.uintptr_t
	persisterCB    C.uintptr_t
	rsmHandle      *C.RsmHandle
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
	Success       bool
	Term          uint64
	ConflictIndex uint64
	ConflictTerm  uint64
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

type PersistentState struct {
	CurrentTerm       uint64
	VotedFor          string
	Log               Log
	SnapshotData      []byte
	LastIncludedIndex uint64
	LastIncludedTerm  uint64
}

type InstallSnapshotArg struct {
	LeaderId          string
	Term              uint64
	LastIncludedIndex uint64
	LastIncludedTerm  uint64
	Data              []byte
}

type InstallSnapshotReply struct {
	Term    uint64
	Success bool
}

func (rf *Raft) GetState() (int, bool) {
	if rf.handle == nil {
		return 0, false
	}
	var term C.int
	var isleader C.int
	if C.raft_get_state(rf.handle, &term, &isleader) != 0 {
		return 0, false
	}
	return int(term), isleader == 1
}

func (rf *Raft) Start(command interface{}) (int, int, bool) {
	if rf.handle == nil {
		return 0, 0, false
	}
	var commandStr string
	if cmd, ok := command.(string); ok {
		commandStr = cmd
	} else {
		commandStr = fmt.Sprintf("%v", command)
	}
	index := 0
	term := 0
	isLeader := false

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
	if r != 0 {
		return 0, 0, false
	}
	return index, term, isLeader
}

func (rf *Raft) Kill() {
	atomic.StoreInt32(&rf.dead, 1)
	if rf.handle == nil {
		return
	}
	handle := rf.handle
	rf.handle = nil
	if rf.rsmHandle != nil {
		C.rsm_kill(rf.rsmHandle)
	} else {
		C.kill_raft(handle)
	}
	// close(rf.applyCh)
	freeCallback(rf.cb)
	freeCallback(rf.channelCb)
	freeCallback(rf.persisterCB)
}

func (rf *Raft) Snapshot(index int, snapshot []byte) {
	if rf.handle == nil {
		return
	}
	if len(snapshot) == 0 {
		return
	}
	C.raft_snapshot(rf.handle, C.uint64_t(index), (*C.char)(unsafe.Pointer(&snapshot[0])), C.int(len(snapshot)))
}

func (rf *Raft) PersistBytes() int {
	return rf.persister.RaftStateSize()
}

func (rf *Raft) RequestVote(args *RequestVoteArg, reply *RequestVoteReply) {
	if rf.handle == nil {
		return
	}
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
	replyBuf := make([]byte, 64)
	n := C.handle_request_vote(rf.handle,
		(*C.char)(unsafe.Pointer(&strArgs[0])), C.int(len(strArgs)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])))

	if int(n) < 0 || int(n) > len(replyBuf) {
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
	replyBuf := make([]byte, 64)
	n := C.handle_append_entries(rf.handle,
		(*C.char)(unsafe.Pointer(&strArgs[0])), C.int(len(strArgs)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])))
	if n < 0 || int(n) > len(replyBuf) {
		fmt.Println("Error: invalid reply size for AppendEntries")
		return
	}
	protoReply := &proto_raft.AppendEntriesReply{}
	err = protobuf.Unmarshal(replyBuf[:n], protoReply)
	if err != nil {
		fmt.Println("Error: failed to unmarshal AppendEntriesReply")
		return
	}
	reply.Success = protoReply.Success
	reply.Term = protoReply.Term
	reply.ConflictIndex = protoReply.ConflictIndex
	reply.ConflictTerm = protoReply.ConflictTerm
}

func (rf *Raft) InstallSnapshot(args *InstallSnapshotArg, reply *InstallSnapshotReply) {
	if rf.handle == nil {
		return
	}
	protoArgs := &proto_raft.InstallSnapshotArg{
		Term:              args.Term,
		LeaderId:          args.LeaderId,
		LastIncludedIndex: args.LastIncludedIndex,
		LastIncludedTerm:  args.LastIncludedTerm,
		Data:              args.Data,
	}
	strArgs, err := protobuf.Marshal(protoArgs)
	if err != nil || len(strArgs) <= 0 {
		fmt.Println("Error: failed to marshal InstallSnapshotArg")
		return
	}
	replyBuf := make([]byte, 64)
	n := C.handle_install_snapshot(rf.handle,
		(*C.char)(unsafe.Pointer(&strArgs[0])), C.int(len(strArgs)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])))
	if n < 0 || int(n) > len(replyBuf) {
		fmt.Println("Error: invalid reply size for InstallSnapshot")
		return
	}
	protoReply := &proto_raft.InstallSnapshotReply{}
	err = protobuf.Unmarshal(replyBuf[:n], protoReply)
	if err != nil {
		fmt.Println("Error: failed to unmarshal InstallSnapshotReply")
		return
	}
	reply.Term = protoReply.Term
	reply.Success = protoReply.Success
}

func Make(peers []*labrpc.ClientEnd, me int, persister *tester.Persister, applyCh chan raftapi.ApplyMsg) raftapi.Raft {
	rf := &Raft{}
	rf.peers = peers
	rf.me = me
	rf.persister = persister
	rf.applyCh = applyCh
	rf.channelCb = registerChannel(rf.applyCh)
	rf.cb = registerLabRpcCallback(peers, &rf.dead)
	rf.persisterCB = registerPersister(rf.persister)
	rf.recChannelCb = rf.channelCb
	rf.closeChannelCb = rf.channelCb
	rf.rsmHandle = nil
	rf.handle = C.create_raft(C.int(me), C.int(len(peers)), rf.cb, rf.channelCb, rf.recChannelCb, rf.closeChannelCb, rf.persisterCB)
	if rf.handle == nil {
		fmt.Println("Go: Failed to create Raft node", me)
		freeCallback(rf.cb)
		freeCallback(rf.channelCb)
		freeCallback(rf.persisterCB)
		return nil
	}
	return rf
}
