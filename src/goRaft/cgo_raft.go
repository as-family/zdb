package raft

/*
#cgo CPPFLAGS: -I${SRCDIR}/../..
#cgo LDFLAGS: -L${SRCDIR}/../../out/build/gcc-14/lib -lzdb_raft
#include "cgo/raft_wrapper.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"time"
	"unsafe"

	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
)

type CppRaft struct {
	handle  *C.RaftHandle
	me      int
	applyCh chan raftapi.ApplyMsg
	killed  bool
}

func MakeCppRaft(peers []*labrpc.ClientEnd, me int, persister *tester.Persister, applyCh chan raftapi.ApplyMsg) *CppRaft {
	// Convert peers to C strings
	cPeers := make([]*C.char, len(peers))
	for i := range peers {
		// Create simple peer ID based on index
		peerStr := C.CString(fmt.Sprintf("peer_%d", i))
		cPeers[i] = peerStr
		defer C.free(unsafe.Pointer(peerStr))
	}

	selfId := C.CString(fmt.Sprintf("peer_%d", me))
	defer C.free(unsafe.Pointer(selfId))

	// For now, pass empty string as persister_id since we're not implementing persistence
	persisterId := C.CString("")
	defer C.free(unsafe.Pointer(persisterId))

	handle := C.raft_create((**C.char)(unsafe.Pointer(&cPeers[0])), C.int(len(cPeers)), C.int(me), persisterId)
	if handle == nil {
		panic("Failed to create C++ Raft instance")
	}

	rf := &CppRaft{
		handle:  handle,
		me:      me,
		applyCh: applyCh,
		killed:  false,
	}

	// Start apply message goroutine
	go rf.applyMessageLoop()

	return rf
}

func (rf *CppRaft) GetState() (int, bool) {
	if rf.killed {
		return 0, false
	}

	var term C.int
	var isLeader C.int

	success := C.raft_get_state(rf.handle, &term, &isLeader)
	if success == 0 {
		return 0, false
	}

	return int(term), isLeader != 0
}

func (rf *CppRaft) Start(command interface{}) (int, int, bool) {
	if rf.killed {
		return -1, -1, false
	}

	// Convert command to string (simple serialization for testing)
	cmdStr := fmt.Sprintf("%v", command)
	cCmd := C.CString(cmdStr)
	defer C.free(unsafe.Pointer(cCmd))

	var index C.int
	var term C.int
	var isLeader C.int

	success := C.raft_start(rf.handle, cCmd, &index, &term, &isLeader)

	return int(index), int(term), success != 0 && isLeader != 0
}

func (rf *CppRaft) Kill() {
	rf.killed = true
	if rf.handle != nil {
		C.raft_kill(rf.handle)
		C.raft_destroy(rf.handle)
		rf.handle = nil
	}
}

func (rf *CppRaft) PersistBytes() int {
	if rf.killed || rf.handle == nil {
		return 0
	}
	return int(C.raft_persist_bytes(rf.handle))
}

func (rf *CppRaft) Snapshot(index int, snapshot []byte) {
	if rf.killed || rf.handle == nil {
		return
	}

	var cSnapshot *C.char
	if len(snapshot) > 0 {
		cSnapshot = (*C.char)(C.CBytes(snapshot))
		defer C.free(unsafe.Pointer(cSnapshot))
	}

	C.raft_snapshot(rf.handle, C.int(index), cSnapshot, C.int(len(snapshot)))
}

func (rf *CppRaft) applyMessageLoop() {
	for !rf.killed {
		var msg C.ApplyMsg
		timeout := C.int(100) // 100ms timeout

		if C.raft_receive_apply_msg(rf.handle, &msg, timeout) == 1 {
			applyMsg := raftapi.ApplyMsg{
				CommandValid:  msg.command_valid != 0,
				CommandIndex:  int(msg.command_index),
				SnapshotValid: msg.snapshot_valid != 0,
				SnapshotTerm:  int(msg.snapshot_term),
				SnapshotIndex: int(msg.snapshot_index),
			}

			if msg.command_valid != 0 && msg.command != nil {
				applyMsg.Command = C.GoString(msg.command)
			}

			if msg.snapshot_valid != 0 && msg.snapshot != nil {
				// For minimal implementation, just handle the basic case
				applyMsg.Snapshot = []byte{}
			}

			select {
			case rf.applyCh <- applyMsg:
			case <-time.After(time.Millisecond * 100):
				// Timeout, continue loop
			}
		}
	}
}
