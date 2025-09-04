package raft

/*
#cgo CPPFLAGS: -I${SRCDIR}/../..
#cgo LDFLAGS: -L${SRCDIR}/../../out/build/gcc-14/lib -lzdb_raft
#include "cgo/raft_wrapper.h"
#include <stdlib.h>
#include <string.h>

// Forward declaration for the export function
extern int goLabrpcCall(int caller_id, int peer_id, char* service_method, void* args_data, int args_size, void* reply_data, int reply_size);
*/
import "C"
import (
	"fmt"
	"sync"
	"time"
	"unsafe"

	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
	"github.com/zdb/proto"
	protobuf "google.golang.org/protobuf/proto"
)

// Global registry for CppRaft instances
var (
	raftInstances = make(map[int]*CppRaft)
	raftMutex     sync.RWMutex
)

type CppRaft struct {
	handle   *C.RaftHandle
	me       int
	peers    []*labrpc.ClientEnd
	applyCh  chan raftapi.ApplyMsg
	killed   bool
	mu       sync.RWMutex  // Protect access to handle and killed state
	stopCh   chan struct{} // Signal to stop goroutines
	killOnce sync.Once     // Ensure Kill() is only called once
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

// Global callback function for C++ to call Go labrpc
//
//export goLabrpcCall
func goLabrpcCall(callerID C.int, peerID C.int, serviceMethod *C.char, argsData unsafe.Pointer, argsSize C.int, replyData unsafe.Pointer, replySize C.int) C.int {
	// Defensive checks
	if serviceMethod == nil || argsData == nil || replyData == nil {
		fmt.Printf("ERROR: Invalid parameters in goLabrpcCall\n")
		return 0
	}

	// Get the service method name
	method := C.GoString(serviceMethod)

	// Get args data
	args := C.GoBytes(argsData, argsSize)

	// fmt.Printf("goLabrpcCall: caller %d -> peer %d, method %s, args size %d, reply size %d\n", int(callerID), int(peerID), method, int(argsSize), int(replySize))

	// Find the CppRaft instance that is making the call
	raftMutex.RLock()
	rf := raftInstances[int(callerID)]
	raftMutex.RUnlock()

	if rf == nil {
		fmt.Printf("ERROR: No CppRaft instance found for caller %d\n", int(callerID))
		return 0
	}

	// Check if the instance is being killed with read lock
	rf.mu.RLock()
	isKilled := rf.killed
	rf.mu.RUnlock()

	if isKilled {
		fmt.Printf("ERROR: CppRaft instance %d is being killed\n", int(callerID))
		return 0
	}

	targetPeer := int(peerID)
	if targetPeer >= len(rf.peers) {
		fmt.Printf("ERROR: Invalid peer ID %d, only have %d peers\n", targetPeer, len(rf.peers))
		return 0
	}

	// Convert protobuf args to Go structs for labrpc
	var goArgs interface{}
	var goReply interface{}

	if method == "CppRaft.RequestVote" {
		protoArgs := &proto.RequestVoteArg{}
		if err := protobuf.Unmarshal(args, protoArgs); err != nil {
			fmt.Printf("ERROR: Failed to unmarshal RequestVoteArg: %v\n", err)
			return 0
		}
		goArgs = &RequestVoteArg{
			CandidateId:  protoArgs.CandidateId,
			Term:         protoArgs.Term,
			LastLogIndex: protoArgs.LastLogIndex,
			LastLogTerm:  protoArgs.LastLogTerm,
		}
		goReply = &RequestVoteReply{}
	} else if method == "CppRaft.AppendEntries" {
		protoArgs := &proto.AppendEntriesArg{}
		if err := protobuf.Unmarshal(args, protoArgs); err != nil {
			fmt.Printf("ERROR: Failed to unmarshal AppendEntriesArg: %v\n", err)
			return 0
		}
		entries := make(Log, len(protoArgs.Entries))
		for i, entry := range protoArgs.Entries {
			entries[i] = LogEntry{
				Index:   entry.Index,
				Term:    entry.Term,
				Command: string(entry.Command),
			}
		}
		goArgs = &AppendEntriesArg{
			LeaderId:     protoArgs.LeaderId,
			Term:         protoArgs.Term,
			PrevLogIndex: protoArgs.PrevLogIndex,
			PrevLogTerm:  protoArgs.PrevLogTerm,
			LeaderCommit: protoArgs.LeaderCommit,
			Entries:      entries,
		}
		goReply = &AppendEntriesReply{}
	} else {
		fmt.Printf("ERROR: Unknown method %s\n", method)
		return 0
	}

	// Make the labrpc call with Go structs
	if rf.peers[targetPeer] == nil {
		fmt.Printf("ERROR: labrpc peer %d is nil\n", targetPeer)
		return 0
	}
	success := rf.peers[targetPeer].Call(method, goArgs, goReply)

	if success {
		// Convert Go reply back to protobuf and serialize
		var replyBytes []byte
		var err error

		if method == "CppRaft.RequestVote" {
			voteReply := goReply.(*RequestVoteReply)
			protoReply := &proto.RequestVoteReply{
				VoteGranted: voteReply.VoteGranted,
				Term:        voteReply.Term,
			}
			replyBytes, err = protobuf.Marshal(protoReply)
		} else if method == "CppRaft.AppendEntries" {
			appendReply := goReply.(*AppendEntriesReply)
			protoReply := &proto.AppendEntriesReply{
				Success: appendReply.Success,
				Term:    appendReply.Term,
			}
			replyBytes, err = protobuf.Marshal(protoReply)
		}

		if err != nil {
			fmt.Printf("ERROR: Failed to marshal reply: %v\n", err)
			return 0
		}

		if len(replyBytes) <= int(replySize) {
			// Copy reply data back
			if len(replyBytes) > 0 {
				C.memmove(replyData, unsafe.Pointer(&replyBytes[0]), C.size_t(len(replyBytes)))
			}
			return C.int(len(replyBytes))
		} else {
			fmt.Printf("ERROR: Reply too large: %d > %d\n", len(replyBytes), int(replySize))
		}
	} else {
		fmt.Printf("ERROR: labrpc call failed from %d to %d, method %s\n", int(callerID), targetPeer, method)
	}

	return 0
}

func registerCppRaftInstance(me int, rf *CppRaft) {
	raftMutex.Lock()
	defer raftMutex.Unlock()
	raftInstances[me] = rf
}

func unregisterCppRaftInstance(me int) {
	raftMutex.Lock()
	defer raftMutex.Unlock()
	delete(raftInstances, me)
}

func MakeCppRaft(peers []*labrpc.ClientEnd, me int, persister *tester.Persister, applyCh chan raftapi.ApplyMsg) *CppRaft {
	// Set up the labrpc callback (only once)
	C.raft_set_labrpc_callback(C.labrpc_call_func(C.goLabrpcCall))

	// Create peer IDs array
	cPeers := make([]*C.char, len(peers))
	for i := range peers {
		peerStr := C.CString(fmt.Sprintf("peer_%d", i))
		cPeers[i] = peerStr
		defer C.free(unsafe.Pointer(peerStr))
	}

	persisterId := C.CString("")
	defer C.free(unsafe.Pointer(persisterId))

	handle := C.raft_create((**C.char)(unsafe.Pointer(&cPeers[0])), C.int(len(cPeers)), C.int(me), persisterId)
	if handle == nil {
		panic("Failed to create C++ Raft instance")
	}

	rf := &CppRaft{
		handle:  handle,
		me:      me,
		peers:   peers,
		applyCh: applyCh,
		killed:  false,
		stopCh:  make(chan struct{}),
	}

	// Register this instance for the callback
	registerCppRaftInstance(me, rf)

	// Start apply message goroutine
	go rf.applyMessageLoop()

	return rf
}

// Implement labrpc service interface methods (called by labrpc framework)
func (rf *CppRaft) RequestVote(args *RequestVoteArg, reply *RequestVoteReply) {
	rf.mu.RLock()
	defer rf.mu.RUnlock()

	if rf.killed || rf.handle == nil {
		return
	}

	protoArgs := &proto.RequestVoteArg{
		CandidateId:  args.CandidateId,
		Term:         args.Term,
		LastLogIndex: args.LastLogIndex,
		LastLogTerm:  args.LastLogTerm,
	}
	argsData, err := protobuf.Marshal(protoArgs)
	if err != nil {
		fmt.Printf("ERROR: Failed to marshal RequestVoteArg: %v\n", err)
		return
	}
	replyBuf := make([]byte, 1024)
	replyLen := C.raft_request_vote_handler(rf.handle,
		(*C.char)(unsafe.Pointer(&argsData[0])), C.int(len(argsData)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])), C.int(len(replyBuf)))
	protoReply := &proto.RequestVoteReply{}
	if replyLen > 0 {
		err := protobuf.Unmarshal(replyBuf[:replyLen], protoReply)
		if err != nil {
			fmt.Printf("ERROR: Failed to unmarshal RequestVoteReply: %v\n", err)
			return
		}
		reply.VoteGranted = protoReply.VoteGranted
		reply.Term = protoReply.Term
	}
}

func (rf *CppRaft) AppendEntries(args *AppendEntriesArg, reply *AppendEntriesReply) {
	rf.mu.RLock()
	defer rf.mu.RUnlock()

	if rf.killed || rf.handle == nil {
		return
	}

	protoEntries := make([]*proto.LogEntry, len(args.Entries))
	for i, entry := range args.Entries {
		protoEntries[i] = &proto.LogEntry{
			Index:   entry.Index,
			Term:    entry.Term,
			Command: []byte(entry.Command),
		}
	}
	protoArgs := &proto.AppendEntriesArg{
		LeaderId:     args.LeaderId,
		Term:         args.Term,
		PrevLogIndex: args.PrevLogIndex,
		PrevLogTerm:  args.PrevLogTerm,
		LeaderCommit: args.LeaderCommit,
		Entries:      protoEntries,
	}
	argsData, err := protobuf.Marshal(protoArgs)
	if err != nil {
		fmt.Printf("ERROR: Failed to marshal AppendEntriesArg: %v\n", err)
		return
	}
	replyBuf := make([]byte, 1024)
	replyLen := C.raft_append_entries_handler(rf.handle,
		(*C.char)(unsafe.Pointer(&argsData[0])), C.int(len(argsData)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])), C.int(len(replyBuf)))

	protoReply := &proto.AppendEntriesReply{}
	if replyLen > 0 {
		err := protobuf.Unmarshal(replyBuf[:replyLen], protoReply)
		if err != nil {
			fmt.Printf("ERROR: Failed to unmarshal AppendEntriesReply: %v\n", err)
			return
		}
		reply.Success = protoReply.Success
		reply.Term = protoReply.Term
	}
}

// GetState returns the current term and whether this server believes it is the leader
func (rf *CppRaft) GetState() (int, bool) {
	rf.mu.RLock()
	defer rf.mu.RUnlock()

	if rf.killed || rf.handle == nil {
		return 0, false
	}

	var term C.int
	var isLeader C.int

	result := C.raft_get_state(rf.handle, &term, &isLeader)
	if result == 0 {
		return 0, false
	}

	return int(term), isLeader == 1
}

// Start begins agreement on a new log entry
func (rf *CppRaft) Start(command interface{}) (int, int, bool) {
	rf.mu.RLock()
	defer rf.mu.RUnlock()

	if rf.killed || rf.handle == nil {
		return 0, 0, false
	}

	cmdStr := fmt.Sprintf("%v", command)
	cCommand := C.CString(cmdStr)
	defer C.free(unsafe.Pointer(cCommand))

	var index, term, isLeader C.int

	result := C.raft_start(rf.handle, cCommand, &index, &term, &isLeader)
	if result == 0 {
		return 0, 0, false
	}

	return int(index), int(term), isLeader == 1
}

// Kill shuts down the Raft instance
func (rf *CppRaft) Kill() {
	rf.killOnce.Do(func() {
		fmt.Printf("Killing RaftHandle for peer_%d\n", rf.me)

		rf.mu.Lock()
		rf.killed = true
		handle := rf.handle
		rf.handle = nil
		rf.mu.Unlock()

		// First unregister to stop new RPCs
		unregisterCppRaftInstance(rf.me)

		// Signal goroutines to stop
		close(rf.stopCh)

		// Give time for in-flight RPCs and goroutines to complete
		time.Sleep(time.Millisecond * 200)

		// Kill and destroy the C++ object
		if handle != nil {
			C.raft_kill(handle)
			fmt.Printf("*****************Destroying RaftImpl for peer_%d\n", rf.me)
			C.raft_destroy(handle)
		}
	})
}

// Killed returns whether Kill() has been called
func (rf *CppRaft) Killed() bool {
	rf.mu.RLock()
	defer rf.mu.RUnlock()
	return rf.killed
}

// Apply message processing loop
func (rf *CppRaft) applyMessageLoop() {
	for {
		select {
		case <-rf.stopCh:
			return
		default:
		}

		rf.mu.RLock()
		if rf.killed || rf.handle == nil {
			rf.mu.RUnlock()
			return
		}
		handle := rf.handle
		rf.mu.RUnlock()

		var msg C.ApplyMsg
		result := C.raft_receive_apply_msg(handle, &msg, 100) // 100ms timeout

		if result == 1 {
			// Convert C message to Go message
			applyMsg := raftapi.ApplyMsg{
				CommandValid:  msg.command_valid == 1,
				CommandIndex:  int(msg.command_index),
				SnapshotValid: msg.snapshot_valid == 1,
				SnapshotTerm:  int(msg.snapshot_term),
				SnapshotIndex: int(msg.snapshot_index),
			}

			if msg.command != nil {
				applyMsg.Command = C.GoString(msg.command)
			}

			if msg.snapshot != nil {
				applyMsg.Snapshot = C.GoBytes(unsafe.Pointer(msg.snapshot), C.int(1024)) // Adjust size as needed
			}

			// Send to application with proper cancellation
			select {
			case rf.applyCh <- applyMsg:
			case <-time.After(time.Millisecond * 100):
				// Timeout sending to application
			case <-rf.stopCh:
				return
			}
		}

		// Check again if we should stop before sleeping
		select {
		case <-rf.stopCh:
			return
		case <-time.After(time.Millisecond * 10):
		}
	}
}

// GetPersistSize returns the size of the persistent state
func (rf *CppRaft) GetPersistSize() int {
	rf.mu.RLock()
	defer rf.mu.RUnlock()

	if rf.killed || rf.handle == nil {
		return 0
	}

	return int(C.raft_persist_bytes(rf.handle))
}

// PersistBytes returns the size of the persistent state (interface requirement)
func (rf *CppRaft) PersistBytes() int {
	return rf.GetPersistSize()
}

// Snapshot takes a snapshot at the given index
func (rf *CppRaft) Snapshot(index int, snapshot []byte) {
	rf.mu.RLock()
	defer rf.mu.RUnlock()

	if rf.killed || rf.handle == nil {
		return
	}

	cSnapshot := C.CString(string(snapshot))
	defer C.free(unsafe.Pointer(cSnapshot))

	C.raft_snapshot(rf.handle, C.int(index), cSnapshot, C.int(len(snapshot)))
}
