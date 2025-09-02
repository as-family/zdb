package raft

import (
	"6.5840/labrpc"
	"6.5840/raftapi"
	tester "6.5840/tester1"
)

// Choose implementation at compile time or runtime
// Set to true to use C++ implementation
var useCppImplementation = true

// Make creates a Raft instance - either Go or C++ implementation
func Make(peers []*labrpc.ClientEnd, me int, persister *tester.Persister, applyCh chan raftapi.ApplyMsg) raftapi.Raft {
	if useCppImplementation {
		return MakeCppRaft(peers, me, persister, applyCh)
	} else {
		// Fallback to Go implementation (would need to import original raft package)
		panic("Go implementation fallback not implemented yet")
	}
}
