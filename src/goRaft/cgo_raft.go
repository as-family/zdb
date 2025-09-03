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
)

// Global registry for CppRaft instances
var (
	raftInstances = make(map[int]*CppRaft)
	raftMutex     sync.RWMutex
)

type CppRaft struct {
	handle  *C.RaftHandle
	me      int
	peers   []*labrpc.ClientEnd
	applyCh chan raftapi.ApplyMsg
	killed  bool
}

// Global callback function for C++ to call Go labrpc
//
//export goLabrpcCall
func goLabrpcCall(callerID C.int, peerID C.int, serviceMethod *C.char, argsData unsafe.Pointer, argsSize C.int, replyData unsafe.Pointer, replySize C.int) C.int {
	// Get the service method name
	method := C.GoString(serviceMethod)

	// Get args data
	args := C.GoBytes(argsData, argsSize)

	// Find the CppRaft instance that is making the call
	raftMutex.RLock()
	rf := raftInstances[int(callerID)]
	raftMutex.RUnlock()

	if rf == nil {
		fmt.Printf("ERROR: No CppRaft instance found for caller %d\n", int(callerID))
		return 0
	}

	targetPeer := int(peerID)
	if targetPeer >= len(rf.peers) {
		fmt.Printf("ERROR: Invalid peer ID %d, only have %d peers\n", targetPeer, len(rf.peers))
		return 0
	}

	// Make the labrpc call
	var reply []byte
	success := rf.peers[targetPeer].Call(method, args, &reply)

	if success && len(reply) <= int(replySize) {
		// Copy reply data back
		if len(reply) > 0 {
			C.memmove(replyData, unsafe.Pointer(&reply[0]), C.size_t(len(reply)))
		}
		return C.int(len(reply))
	}

	if !success {
		fmt.Printf("ERROR: labrpc call failed for peer %d, method %s\n", targetPeer, method)
	} else {
		fmt.Printf("ERROR: Reply too large: %d > %d\n", len(reply), int(replySize))
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
	}

	// Register this instance for the callback
	registerCppRaftInstance(me, rf)

	// Start apply message goroutine
	go rf.applyMessageLoop()

	return rf
}

// Implement labrpc service interface methods (called by labrpc framework)
func (rf *CppRaft) RequestVote(args []byte, reply *[]byte) {
	replyBuf := make([]byte, 1024)
	replyLen := C.raft_request_vote_handler(rf.handle,
		(*C.char)(unsafe.Pointer(&args[0])), C.int(len(args)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])), C.int(len(replyBuf)))

	if replyLen > 0 {
		*reply = replyBuf[:int(replyLen)]
	}
}

func (rf *CppRaft) AppendEntries(args []byte, reply *[]byte) {
	replyBuf := make([]byte, 1024)
	replyLen := C.raft_append_entries_handler(rf.handle,
		(*C.char)(unsafe.Pointer(&args[0])), C.int(len(args)),
		(*C.char)(unsafe.Pointer(&replyBuf[0])), C.int(len(replyBuf)))

	if replyLen > 0 {
		*reply = replyBuf[:int(replyLen)]
	}
}

// ConnectAllPeers establishes connections to all peer servers (no-op for labrpc)
func (rf *CppRaft) ConnectAllPeers() {
	C.raft_connect_all_peers(rf.handle)
}

// GetState returns the current term and whether this server believes it is the leader
func (rf *CppRaft) GetState() (int, bool) {
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
	if !rf.killed {
		rf.killed = true
		C.raft_kill(rf.handle)
		unregisterCppRaftInstance(rf.me)
	}
}

// Killed returns whether Kill() has been called
func (rf *CppRaft) Killed() bool {
	return rf.killed
}

// Apply message processing loop
func (rf *CppRaft) applyMessageLoop() {
	for !rf.killed {
		var msg C.ApplyMsg
		result := C.raft_receive_apply_msg(rf.handle, &msg, 100) // 100ms timeout

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

			// Send to application
			select {
			case rf.applyCh <- applyMsg:
			case <-time.After(time.Millisecond * 100):
				// Timeout sending to application
			}
		}

		time.Sleep(time.Millisecond * 10)
	}
}

// GetPersistSize returns the size of the persistent state
func (rf *CppRaft) GetPersistSize() int {
	return int(C.raft_persist_bytes(rf.handle))
}

// PersistBytes returns the size of the persistent state (interface requirement)
func (rf *CppRaft) PersistBytes() int {
	return int(C.raft_persist_bytes(rf.handle))
}

// Snapshot takes a snapshot at the given index
func (rf *CppRaft) Snapshot(index int, snapshot []byte) {
	cSnapshot := C.CString(string(snapshot))
	defer C.free(unsafe.Pointer(cSnapshot))

	C.raft_snapshot(rf.handle, C.int(index), cSnapshot, C.int(len(snapshot)))
}
